#define __NO_VERSION__
#include <linux/config.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
#define PMAC_SUPPORT_PCM_BEEP
#endif

#include "../alsa-kernel/ppc/pmac.c"

#ifdef PMAC_SUPPORT_PCM_BEEP
/*
 * beep using pcm
 */
struct snd_pmac_beep {
	int running;	/* boolean */
	int volume;	/* mixer volume: 0-100 */
	int volume_play;	/* currently playing volume */
	int hz;
	int nsamples;
	short *buf;		/* allocated wave buffer */
	unsigned long addr;	/* physical address of buffer */
	struct timer_list timer;	/* timer list for stopping beep */
	void (*orig_mksound)(unsigned int, unsigned int);
				/* pointer to restore */
	snd_kcontrol_t *control;	/* mixer element */
};

/*
 * stop beep if running (no spinlock!)
 */
static void snd_pmac_beep_stop(pmac_t *chip)
{
	pmac_beep_t *beep = chip->beep;

	if (beep && beep->running) {
		beep->running = 0;
		del_timer(&beep->timer);
		snd_pmac_dma_stop(&chip->playback);
		st_le16(&chip->extra_dma.cmds->command, DBDMA_STOP);
	}
}

/*
 * Stuff for outputting a beep.  The values range from -327 to +327
 * so we can multiply by an amplitude in the range 0..100 to get a
 * signed short value to put in the output buffer.
 */
static short beep_wform[256] = {
	0,	40,	79,	117,	153,	187,	218,	245,
	269,	288,	304,	316,	323,	327,	327,	324,
	318,	310,	299,	288,	275,	262,	249,	236,
	224,	213,	204,	196,	190,	186,	183,	182,
	182,	183,	186,	189,	192,	196,	200,	203,
	206,	208,	209,	209,	209,	207,	204,	201,
	197,	193,	188,	183,	179,	174,	170,	166,
	163,	161,	160,	159,	159,	160,	161,	162,
	164,	166,	168,	169,	171,	171,	171,	170,
	169,	167,	163,	159,	155,	150,	144,	139,
	133,	128,	122,	117,	113,	110,	107,	105,
	103,	103,	103,	103,	104,	104,	105,	105,
	105,	103,	101,	97,	92,	86,	78,	68,
	58,	45,	32,	18,	3,	-11,	-26,	-41,
	-55,	-68,	-79,	-88,	-95,	-100,	-102,	-102,
	-99,	-93,	-85,	-75,	-62,	-48,	-33,	-16,
	0,	16,	33,	48,	62,	75,	85,	93,
	99,	102,	102,	100,	95,	88,	79,	68,
	55,	41,	26,	11,	-3,	-18,	-32,	-45,
	-58,	-68,	-78,	-86,	-92,	-97,	-101,	-103,
	-105,	-105,	-105,	-104,	-104,	-103,	-103,	-103,
	-103,	-105,	-107,	-110,	-113,	-117,	-122,	-128,
	-133,	-139,	-144,	-150,	-155,	-159,	-163,	-167,
	-169,	-170,	-171,	-171,	-171,	-169,	-168,	-166,
	-164,	-162,	-161,	-160,	-159,	-159,	-160,	-161,
	-163,	-166,	-170,	-174,	-179,	-183,	-188,	-193,
	-197,	-201,	-204,	-207,	-209,	-209,	-209,	-208,
	-206,	-203,	-200,	-196,	-192,	-189,	-186,	-183,
	-182,	-182,	-183,	-186,	-190,	-196,	-204,	-213,
	-224,	-236,	-249,	-262,	-275,	-288,	-299,	-310,
	-318,	-324,	-327,	-327,	-323,	-316,	-304,	-288,
	-269,	-245,	-218,	-187,	-153,	-117,	-79,	-40,
};

#define BEEP_SRATE	22050	/* 22050 Hz sample rate */
#define BEEP_BUFLEN	512
#define BEEP_VOLUME	15	/* 0 - 100 */

static void snd_pmac_beep_stop_callback(unsigned long data)
{
	pmac_t *chip = snd_magic_cast(pmac_t, (void*)data,);

	spin_lock(&chip->reg_lock);
	snd_pmac_beep_stop(chip);
	snd_pmac_pcm_set_format(chip);
	spin_unlock(&chip->reg_lock);
}

/* because mksound callback takes no private argument, we must keep
   the chip pointer here as static variable.
   This means that only one chip can beep.  Well, it's ok -
   anyway we don't like hearing loud beeps from every chip
   at the same time :)
*/
   
static pmac_t *beeping_chip = NULL;

static void snd_pmac_mksound(unsigned int hz, unsigned int ticks)
{
	pmac_t *chip;
	pmac_stream_t *rec;
	pmac_beep_t *beep;
	unsigned long flags;
	int beep_speed = 0;
	int srate;
	int period, ncycles, nsamples;
	int i, j, f;
	short *p;

	if ((chip = beeping_chip) == NULL || (beep = chip->beep) == NULL)
		return;
	rec = &chip->playback;

	beep_speed = snd_pmac_rate_index(chip, rec, BEEP_SRATE);
	srate = chip->freq_table[beep_speed];

	if (hz <= srate / BEEP_BUFLEN || hz > srate / 2) {
		/* this is a hack for broken X server code */
		hz = 750;
		ticks = 12;
	}

	spin_lock_irqsave(&chip->reg_lock, flags);
	if (chip->playback.running || chip->capture.running || beep->running) {
		spin_unlock_irqrestore(&chip->reg_lock, flags);
		return;
	}
	beep->running = 1;
	spin_unlock_irqrestore(&chip->reg_lock, flags);

	if (hz == beep->hz && beep->volume == beep->volume_play) {
		nsamples = beep->nsamples;
	} else {
		period = srate * 256 / hz;	/* fixed point */
		ncycles = BEEP_BUFLEN * 256 / period;
		nsamples = (period * ncycles) >> 8;
		f = ncycles * 65536 / nsamples;
		j = 0;
		p = beep->buf;
		for (i = 0; i < nsamples; ++i, p += 2) {
			p[0] = p[1] = beep_wform[j >> 8] * beep->volume;
			j = (j + f) & 0xffff;
		}
		beep->hz = hz;
		beep->volume_play = beep->volume;
		beep->nsamples = nsamples;
	}

	spin_lock_irqsave(&chip->reg_lock, flags);
	if (beep->running) {
		if (ticks <= 0)
			ticks = 1;
		del_timer(&beep->timer);
		beep->timer.expires = jiffies + ticks;
		beep->timer.function = snd_pmac_beep_stop_callback;
		beep->timer.data = (unsigned long)chip;
		add_timer(&beep->timer);
		snd_pmac_dma_stop(rec);
		st_le16(&chip->extra_dma.cmds->req_count, nsamples * 4);
		st_le16(&chip->extra_dma.cmds->xfer_status, 0);
		st_le32(&chip->extra_dma.cmds->cmd_dep, chip->extra_dma.addr);
		st_le32(&chip->extra_dma.cmds->phy_addr, beep->addr);
		st_le16(&chip->extra_dma.cmds->command, OUTPUT_MORE + BR_ALWAYS);
		out_le32(&chip->awacs->control,
			 (in_le32(&chip->awacs->control) & ~0x1f00)
			 | (beep_speed << 8));
		out_le32(&chip->awacs->byteswap, 0);
		snd_pmac_dma_set_command(rec, &chip->extra_dma);
		snd_pmac_dma_run(rec, RUN);
	}
	spin_unlock_irqrestore(&chip->reg_lock, flags);
}

/*
 * beep volume mixer
 */
static int snd_pmac_info_beep(snd_kcontrol_t *kcontrol, snd_ctl_elem_info_t *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 100;
	return 0;
}

static int snd_pmac_get_beep(snd_kcontrol_t *kcontrol, snd_ctl_elem_value_t *ucontrol)
{
	pmac_t *chip = snd_kcontrol_chip(kcontrol);
	snd_runtime_check(chip->beep, return -ENXIO);
	ucontrol->value.integer.value[0] = chip->beep->volume;
	return 0;
}

static int snd_pmac_put_beep(snd_kcontrol_t *kcontrol, snd_ctl_elem_value_t *ucontrol)
{
	pmac_t *chip = snd_kcontrol_chip(kcontrol);
	int oval;
	snd_runtime_check(chip->beep, return -ENXIO);
	oval = chip->beep->volume;
	chip->beep->volume = ucontrol->value.integer.value[0];
	return oval != chip->beep->volume;
}

static snd_kcontrol_new_t snd_pmac_beep_mixer = {
	.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
	.name = "Beep Playback Volume",
	.index = 0,
	.info = snd_pmac_info_beep,
	.get = snd_pmac_get_beep,
	.put = snd_pmac_put_beep,
};

static void snd_pmac_beep_free(snd_kcontrol_t *control)
{
	pmac_t *chip = snd_magic_cast(pmac_t, _snd_kcontrol_chip(control),);
	if (chip->beep) {
		/* restore */
		kd_mksound = chip->beep->orig_mksound;
		kfree(chip->beep->buf);
		kfree(chip->beep);
		chip->beep = NULL;
	}
}

/* Initialize beep stuff */
int __init snd_pmac_attach_beep(pmac_t *chip)
{
	pmac_beep_t *beep;
	int err;

	beep = kmalloc(sizeof(*beep), GFP_KERNEL);
	if (! beep)
		return -ENOMEM;

	beep->buf = (short *) kmalloc(BEEP_BUFLEN * 4, GFP_KERNEL);
	if (! beep->buf) {
		kfree(beep);
		return -ENOMEM;
	}
	beep->addr = virt_to_bus(beep->buf);
	init_timer(&beep->timer);
	beep->timer.function = snd_pmac_beep_stop_callback;
	beep->timer.data = (unsigned long) chip;
	beep->orig_mksound = kd_mksound;
	beep->volume = BEEP_VOLUME;
	beep->running = 0;
	beep->control = snd_ctl_new1(&snd_pmac_beep_mixer, chip);
	if (beep->control == NULL) {
		kfree(beep);
		return -ENOMEM;
	}
	beep->control->private_free = snd_pmac_beep_free;
	if ((err = snd_ctl_add(chip->card, beep->control)) < 0) {
		kfree(beep);
		return err;
	}

	/* hook */
	beeping_chip = chip;
	chip->beep = beep;
	kd_mksound = snd_pmac_mksound;

	return 0;
}

#endif /* PMAC_SUPPORT_PCM_BEEP */
