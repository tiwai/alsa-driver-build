/*
 * ALSA driver for the Aureal Vortex family of soundprocessors.
 * Author: Manuel Jander (mjander@embedded.cl)
 *
 *   This driver is the result of the OpenVortex Project from Savannah
 * (savannah.nongnu.org/projects/openvortex). I would like to thank
 * the developers of OpenVortex, Jeff Muizelar and Kester Maddock, from
 * whom i got plenty of help, and their codebase was invaluable.
 *   Thanks to the ALSA developers, they helped a lot working out
 * the ALSA part.
 *   Thanks also to Sourceforge for maintaining the old binary drivers,
 * and the forum, where developers could comunicate.
 *
 * Now at least i can play Legacy DOOM with MIDI music :-)
 */

#include "au88x0.h"
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#define SNDRV_GET_ID
#include <sound/initval.h>

// module parameters (see "Module Parameters")
static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;

MODULE_PARM(index, "1-" __MODULE_STRING(SNDRV_CARDS) "i");
MODULE_PARM_DESC(index, "Index value for " CARD_NAME " soundcard.");
MODULE_PARM_SYNTAX(index, SNDRV_INDEX_DESC);
MODULE_PARM(id, "1-" __MODULE_STRING(SNDRV_CARDS) "s");
MODULE_PARM_DESC(id, "ID string for " CARD_NAME " soundcard.");
MODULE_PARM_SYNTAX(id, SNDRV_ID_DESC);
MODULE_PARM(enable, "1-" __MODULE_STRING(SNDRV_CARDS) "i");
MODULE_PARM_DESC(enable, "Enable " CARD_NAME " soundcard.");
MODULE_PARM_SYNTAX(enable, SNDRV_ENABLE_DESC);

MODULE_DESCRIPTION("Aureal vortex");
MODULE_CLASSES("{sound}");
MODULE_LICENSE("GPL");
MODULE_DEVICES("{{Aureal Semiconductor Inc., Aureal Vortex Sound Processor}}");

#ifndef MODULE
/* format is: snd-mychip=enable,index,id */
static int __init alsa_card_vortex_setup(char *str) {
    static unsigned __initdata nr_dev = 0;

    if (nr_dev >= SNDRV_CARDS)
        return 0;
    (void) (get_option(&str, &enable[nr_dev]) == 2 &&
            get_option(&str, &index[nr_dev]) == 2 &&
            get_id(&str, &id[nr_dev]) == 2);
    nr_dev++;
    return 1;
}
__setup("snd-au88x0=", alsa_card_vortex_setup);
#endif /* ifndef MODULE */


MODULE_DEVICE_TABLE(pci, snd_vortex_ids);


// component-destructor
// (see "Management of Cards and Components")
static int snd_vortex_dev_free(snd_device_t *device) {
    vortex_t *vortex = snd_magic_cast(vortex_t, device->device_data,
                                      return -ENXIO);
    
    vortex_gameport_unregister(vortex);
    vortex_core_shutdown(vortex);
    // Take down PCI interface.
    synchronize_irq(vortex->irq);
    free_irq(vortex->irq, vortex);
    iounmap((void*)vortex->mmio);
    release_resource(vortex->mmio_res);
    kfree_nocheck(vortex->mmio_res);
    pci_disable_device(vortex->pci_dev);
    snd_magic_kfree(vortex);

    return 0;
}

// chip-specific constructor
// (see "Management of Cards and Components")
static int __devinit
snd_vortex_create(snd_card_t *card, struct pci_dev *pci, vortex_t **rchip) {
    vortex_t *chip;
    int err;
    static snd_device_ops_t ops = {
        .dev_free = snd_vortex_dev_free,
    };

    *rchip = NULL;

    // check PCI availability (DMA).
    if ((err = pci_enable_device(pci)) < 0)
        return err;
    if (!pci_dma_supported(pci, VORTEX_DMA_MASK)) {
        printk(KERN_ERR "error to set DMA mask\n");
        return -ENXIO;
    }
    pci_set_dma_mask(pci, VORTEX_DMA_MASK);

    chip = snd_magic_kcalloc(vortex_t, 0, GFP_KERNEL);
    if (chip == NULL)
        return -ENOMEM;

    chip->card = card;

    // initialize the stuff
    chip->pci_dev = pci;
    chip->io = pci_resource_start(pci, 0);
    chip->vendor = pci->vendor;
    chip->device = pci->device;
    chip->card = card;
    chip->irq = -1;

    // (1) PCI resource allocation
    // Get MMIO area
    //
    chip->mmio_res = request_mem_region(pci_resource_start(pci, 0), pci_resource_len(pci, 0), CARD_NAME_SHORT);
    if (! chip->mmio_res) {
	err = -EBUSY;
        goto regions_out;
    }
    chip->mmio = (unsigned long) ioremap_nocache(pci_resource_start(pci,0), pci_resource_len(pci,0));
    if (!chip->mmio) {
        printk(KERN_ERR "MMIO area remap failed.\n");
        err = -ENOMEM;
        goto ioremap_out;
    }

    /* Init audio core.
     * This must be done before we do request_irq otherwise we can get spurious
     * interupts that we do not handle properly and make a mess of things */
    if ((err = vortex_core_init(chip)) != 0) {
	printk(KERN_ERR "hw core init failed\n");
	goto core_out;
    }
    
    if ((err = request_irq(pci->irq, vortex_interrupt, SA_INTERRUPT | SA_SHIRQ,
                    CARD_NAME_SHORT, (void *) chip)) != 0) {
        printk(KERN_ERR "cannot grab irq\n");
        goto irq_out;
    }
    chip->irq = pci->irq;
    
    pci_set_master(pci);
    // End of PCI setup.


    // Register alsa root device.
    if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops)) < 0) {
    	goto alloc_out;
    }

    *rchip = chip;

    return 0;
    
alloc_out:
    synchronize_irq(chip->irq);
    free_irq(chip->irq, chip);
irq_out:
    vortex_core_shutdown(chip);
core_out:
    //FIXME: the type of chip->mmio might need to be changed??
    iounmap((void*)chip->mmio);
ioremap_out:
    pci_release_regions(chip->pci_dev);
regions_out:
    pci_disable_device(chip->pci_dev);
    //FIXME: this not the right place to unregister the gameport
    vortex_gameport_unregister(chip);
    return err;
}

// constructor -- see "Constructor" sub-section
static int __devinit
snd_vortex_probe(struct pci_dev *pci, const struct pci_device_id *pci_id) {
    static int dev;
    snd_card_t *card;
    vortex_t *chip;
    int err;

    // (1)
    if (dev >= SNDRV_CARDS)
        return -ENODEV;
    if (!enable[dev]) {
        dev++;
        return -ENOENT;
    }
    // (2)
    card = snd_card_new(index[dev], id[dev], THIS_MODULE, 0);
    if (card == NULL)
        return -ENOMEM;

    // (3)
    if ((err = snd_vortex_create(card, pci, &chip)) < 0) {
        snd_card_free(card);
        return err;
    }
    // (4) Alloc components.
    // ADB pcm.
    if ((err = snd_vortex_new_pcm(chip, VORTEX_PCM_ADB, NR_ADB)) < 0) {
        snd_card_free(card);
        return err;
    }
#ifndef CHIP_AU8820
	// ADB SPDIF
    if ((err = snd_vortex_new_pcm(chip, VORTEX_PCM_SPDIF, 1)) < 0) {
        snd_card_free(card);
        return err;
    }	
#endif
	/*
	// ADB I2S
    if ((err = snd_vortex_new_pcm(chip, VORTEX_PCM_I2S, 1)) < 0) {
        snd_card_free(card);
        return err;
    }
	*/
#ifndef CHIP_AU8810
    // WT pcm.
    if ((err = snd_vortex_new_pcm(chip, VORTEX_PCM_WT, NR_WT)) < 0) {
        snd_card_free(card);
        return err;
    }
#endif
    // snd_ac97_mixer and Vortex mixer.
    if ((err = snd_vortex_mixer(chip)) < 0) {
        snd_card_free(card);
        return err;
    }
    if ((err = snd_vortex_midi(chip)) < 0) {
        snd_card_free(card);
        return err;
    }
    if ((err = vortex_gameport_register(chip)) < 0) {
        snd_card_free(card);
        return err;
    }
#if 0
    if (snd_seq_device_new(card, 1, SNDRV_SEQ_DEV_ID_VORTEX_SYNTH,
                           sizeof(snd_vortex_synth_arg_t), &wave) < 0
        || wave == NULL) {
        snd_printk("Can't initialize Aureal wavetable synth\n");
    } else {
        snd_vortex_synth_arg_t *arg;

        arg = SNDRV_SEQ_DEVICE_ARGPTR(wave);
        strcpy(wave->name, "Aureal Synth");
        arg->hwptr = vortex;
        arg->index = 1;
        arg->seq_ports = seq_ports[dev];
        arg->max_voices = max_synth_voices[dev];
    }
#endif

    // (5)
    strcpy(card->driver, "Aureal Vortex");
    strcpy(card->shortname, CARD_NAME_SHORT);
    sprintf(card->longname, "%s at 0x%lx irq %i",
            card->shortname, chip->io, chip->irq);
    
#ifdef CHIP_AU8830
    {
        unsigned char revision;
        if ((err = pci_read_config_byte(pci, PCI_REVISION_ID, &revision)) < 0) {
            snd_card_free(card);
            return err;
    	}

	if (revision != 0xfe && revision != 0xfa) {
            printk(KERN_ALERT "vortex: The revision (%x) of your card has not been seen before.\n", revision);
            printk(KERN_ALERT "vortex: Please email the results of 'lspci -vv' to openvortex-dev@nongnu.org.\n");
            snd_card_free(card);
            err = -ENODEV;
            return err;
        }
    }
#endif
    // (6)
    if ((err = snd_card_register(card)) < 0) {
        snd_card_free(card);
        return err;
    }
    // (7)
    pci_set_drvdata(pci, chip);
    dev++;
	vortex_connect_default(chip, 1);
    vortex_enable_int(chip);
    return 0;
}

// destructor -- see "Destructor" sub-section
static void __devexit snd_vortex_remove(struct pci_dev *pci) {
    vortex_t *vortex = snd_magic_cast(vortex_t,
                                      pci_get_drvdata(pci), return);

    if (vortex) {
        // Release ALSA stuff.
        snd_card_free(vortex->card);
        // Free Vortex struct.
        pci_set_drvdata(pci, NULL);
    } else
        printk("snd_vortex_remove called more than one time!\n");
}

// pci_driver definition
static struct pci_driver driver = {
    .name = CARD_NAME_SHORT,
    .id_table = snd_vortex_ids,
    .probe = snd_vortex_probe,
    .remove = __devexit_p(snd_vortex_remove),
};

// initialization of the module
static int __init alsa_card_vortex_init(void) {
    int err;

    if ((err = pci_module_init(&driver)) < 0) {
#ifdef MODULE
        printk(KERN_ERR "Aureal soundcard not found " "or device busy\n");
#endif
        return err;
    }
    return 0;
}

// clean up the module
static void __exit alsa_card_vortex_exit(void) {
    pci_unregister_driver(&driver);
}

module_init(alsa_card_vortex_init)
module_exit(alsa_card_vortex_exit)
