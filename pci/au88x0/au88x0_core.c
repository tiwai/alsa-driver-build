/*
    Vortex core low level functions.
	
 Author: Manuel Jander (mjander@users.sourceforge.cl)
 These functions are mainly the result of translations made
 from the original disassembly of the au88x0 binary drivers,
 written by Aureal before they went down.
 Many thanks to the Jeff Muizelar, Kester Maddock, and whoever
 contributed to the OpenVortex project.
 The author of this file, put the few available pieces together
 and translated the rest of the riddle (Mix, Src and connection stuff).
 Some things are still to be discovered, and their meanings are unclear.

 Some of these functions aren't intended to be really used, rather
 to help to understand how does the AU88X0 chips work. Keep them in, because
 they could be used somewhere in the future.

 This code hasn't been tested or proof read thoroughly. If you wanna help,
 take a look at the AU88X0 assembly and check if this matches.
 Functions tested ok so far are (they show the desired effect
 at least):
   vortex_routes(); (1 bug fixed).
   vortex_adb_addroute();
   vortex_adb_addroutes();
   vortex_connect_codecplay();
   vortex_src_flushbuffers();
   vortex_adbdma_setmode();  note: still some unknown arguments!
   vortex_adbdma_startfifo();
   vortex_adbdma_stopfifo();
   vortex_fifo_setadbctrl(); note: still some unknown arguments!
   vortex_mix_setinputvolumebyte();
   vortex_mix_enableinput();
   vortex_mixer_addWTD(); (fixed)
   vortex_connection_adbdma_src_src();
   vortex_connection_adbdma_src();
   vortex_src_change_convratio();
   vortex_src_addWTD(); (fixed)

 History:

 01-03-2003 First revision.
 01-21-2003 Some bug fixes.
 17-02-2003 many bugfixes after a big versioning mess.
 18-02-2003 JAAAAAHHHUUUUUU!!!! The mixer works !! I'm just so happy !
			 (2 hours later...) I cant believe it! Im really lucky today.
			 Now the SRC is working too! Yeah! XMMS works !
 20-02-2003 First steps into the ALSA world.
 28-02-2003 As my birthday present, i discovered how the DMA buffer pages really
            work :-). It was all wrong.
 12-03-2003 ALSA driver starts working (2 channels).
 16-03-2003 More srcblock_setupchannel discoveries.
 12-04-2003 AU8830 playback support. Recording in the works.
 17-04-2003 vortex_route() and vortex_routes() bug fixes. AU8830 recording
 			works now, but chipn' dale effect is still there.
 16-05-2003 SrcSetupChannel cleanup. Moved the Src setup stuff entirely
            into au88x0_pcm.c .
 06-06-2003 Buffer shifter bugfix. Mixer volume fix.
 
*/

#include "au88x0.h"
#include <linux/delay.h>


/*  MIXER (CAsp4Mix.s and CAsp4Mixer.s) */

// FIXME: get rid of this.
int mchannels[NR_MIXIN];
int rampchs[NR_MIXIN];

void vortex_mixer_en_sr(vortex_t *vortex, int channel) {
    hwwrite(vortex->mmio, VORTEX_MIXER_SR, hwread(vortex->mmio, VORTEX_MIXER_SR) | (0x1 << channel));
}
void vortex_mixer_dis_sr(vortex_t *vortex, int channel) {
    hwwrite(vortex->mmio, VORTEX_MIXER_SR,
	    hwread(vortex->mmio, VORTEX_MIXER_SR) & ~(0x1 << channel));
}

void vortex_mix_muteinputgain(vortex_t *vortex, unsigned char mix, unsigned char channel) {
    hwwrite(vortex->mmio, VORTEX_MIX_INVOL_A + ((mix << 5) + channel), 0x80);
    hwwrite(vortex->mmio, VORTEX_MIX_INVOL_B + ((mix << 5) + channel), 0x80);
}

int  vortex_mix_getvolume(vortex_t *vortex, unsigned char mix) {
    int a;
    a = hwread(vortex->mmio, VORTEX_MIX_VOL_A + (mix << 2)) & 0xff;
    //FP2LinearFrac(a);
    return (a);
}

void vortex_mix_setvolumebyte(vortex_t *vortex, unsigned char mix, unsigned char vol) {
    int temp;
    hwwrite(vortex->mmio, VORTEX_MIX_VOL_A + (mix << 2), vol);
    if (1) {			/*if (this_10) */
		temp = hwread(vortex->mmio, VORTEX_MIX_VOL_B + (mix << 2));
		if ((temp != 0x80) || (vol == 0x80))
	    	return;
    }
    hwwrite(vortex->mmio, VORTEX_MIX_VOL_B + (mix << 2), vol);
}

int  vortex_mix_getinputvolume(vortex_t *vortex, unsigned char mix, int channel, int *vol) {
    int a;
    if (!(mchannels[mix] & (1 << channel)))
	return 0;
    a = hwread(vortex->mmio,
	       VORTEX_MIX_INVOL_A + (((mix << 5) + channel) << 2));
    /*
       if (rampchs[mix] == 0)
       a = FP2LinearFrac(a);
       else
       a = FP2LinearFracWT(a);
     */
    *vol = a;
    return (0);
}

void vortex_mix_setinputvolumebyte(vortex_t *vortex, unsigned char mix, int mixin, unsigned char vol) {
    int temp;

    hwwrite(vortex->mmio, VORTEX_MIX_INVOL_A + (((mix << 5) + mixin) << 2), vol);
    if (1) {			/* this_10, initialized to 1. */
    	temp = hwread(vortex->mmio, VORTEX_MIX_INVOL_B + (((mix << 5) + mixin) << 2));
    	if ((temp != 0x80) || (vol == 0x80))
	        return;
    }
    hwwrite(vortex->mmio, VORTEX_MIX_INVOL_B + (((mix << 5) + mixin) << 2), vol);
}

int  vortex_mix_getenablebit(vortex_t *vortex, unsigned char mix, int mixin) {
    int addr, temp;
    if (mixin >= 0)
	    addr = mixin;
    else
	    addr = mixin + 3;
    addr = ((mix << 3) + (addr >> 2)) << 2;
    temp = hwread(vortex->mmio, VORTEX_MIX_ENIN + addr);
    return ((temp >> (mixin & 3)) & 1);
}

void vortex_mix_setenablebit(vortex_t *vortex, unsigned char mix, int mixin, int en) {
    int temp, addr;

    if (mixin < 0)
        addr = (mixin + 3);
    else
	    addr = mixin;
    addr = ((mix << 3) + (addr >> 2)) << 2;
    temp = hwread(vortex->mmio, VORTEX_MIX_ENIN + addr);
    if (en)
	    temp |= (1 << (mixin & 3));
    else
	    temp &= ~(1 << (mixin & 3));
    /* Mute input. Avoid crackling? */
    hwwrite(vortex->mmio, VORTEX_MIX_INVOL_B + (((mix << 5) + mixin) << 2), 0x80);
    /* No idea what this does. */
    hwwrite(vortex->mmio, VORTEX_MIX_U0 + (mixin << 2), 0x0);
    hwwrite(vortex->mmio, VORTEX_MIX_U0 + 4 + (mixin << 2), 0x0);
    /* Write enable bit. */
    hwwrite(vortex->mmio, VORTEX_MIX_ENIN + addr, temp);
}

void vortex_mix_killinput(vortex_t *vortex, unsigned char mix, int mixin) {
    rampchs[mix] &= ~(1 << mixin);
    vortex_mix_setinputvolumebyte(vortex, mix, mixin, 0x80);
    mchannels[mix] &= ~(1 << mixin);
    vortex_mix_setenablebit(vortex, mix, mixin, 0);
}

void vortex_mix_enableinput(vortex_t *vortex, unsigned char mix, int mixin) {
    vortex_mix_killinput(vortex, mix, mixin);
    if ((mchannels[mix] & (1 << mixin)) == 0) {
	    vortex_mix_setinputvolumebyte(vortex, mix, mixin, 0x80);	/*0x80 : mute */
	    mchannels[mix] |= (1 << mixin);
    }
    vortex_mix_setenablebit(vortex, mix, mixin, 1);
}

void vortex_mix_disableinput(vortex_t *vortex, unsigned char mix, int channel, int ramp) {
    if (ramp) {
	    rampchs[mix] |= (1 << channel);
	    // Register callback.
	    //vortex_mix_startrampvolume(vortex);
	    vortex_mix_killinput(vortex, mix, channel);
    } else
	    vortex_mix_killinput(vortex, mix, channel);
}

int  vortex_mixer_addWTD(vortex_t *vortex, unsigned char mix, unsigned char ch) {
    int temp, lifeboat = 0, prev;


    temp = hwread(vortex->mmio, VORTEX_MIXER_SR);
    if ((temp & (1 << ch)) == 0) {
	    hwwrite(vortex->mmio, VORTEX_MIXER_CHNBASE + (ch << 2), mix);
	    vortex_mixer_en_sr(vortex, ch);
	    return 1;
    }
    prev = VORTEX_MIXER_CHNBASE + (ch << 2);
    temp = hwread(vortex->mmio, prev);
    while (temp & 0x10) {
	    prev = VORTEX_MIXER_RTBASE + ((temp & 0xf) << 2);
	    temp = hwread(vortex->mmio, prev);
		//printk(KERN_INFO "vortex: mixAddWTD: while addr=%x, val=%x\n", prev, temp);
	    if ((++lifeboat) > 0xf) {
    	    printk(KERN_ERR "vortex_mixer_addWTD: lifeboat overflow\n");
	        return 0;
	    }
    }
    hwwrite(vortex->mmio, VORTEX_MIXER_RTBASE + ((temp & 0xf) << 2), mix);
    hwwrite(vortex->mmio, prev, (temp & 0xf) | 0x10);
    return 1;
}

int  vortex_mixer_delWTD(vortex_t *vortex, unsigned char mix, unsigned char ch) {
	int esp14=-1, esp18, eax, ebx, edx, ebp, esi=0;
	//int esp1f=edi(while)=src, esp10=ch;

    eax = hwread(vortex->mmio, VORTEX_MIXER_SR);
    if (((1 << ch) & eax) == 0) {
		printk(KERN_ERR "mix ALARM %x\n", eax);
		return 0;
	}
	ebp = VORTEX_MIXER_CHNBASE + (ch << 2);
    esp18 = hwread(vortex->mmio, ebp);
    if (esp18 & 0x10) {
		ebx = (esp18 & 0xf);
		if (mix == ebx) {
			ebx = VORTEX_MIXER_RTBASE + (mix << 2);
			edx = hwread(vortex->mmio, ebx);
			//7b60
			hwwrite(vortex->mmio, ebp, edx);
			hwwrite(vortex->mmio, ebx, 0);
		} else {
			//7ad3
			edx = hwread(vortex->mmio, VORTEX_MIXER_RTBASE + (ebx << 2));
			//printk(KERN_INFO "vortex: mixdelWTD: 1 addr=%x, val=%x, src=%x\n", ebx, edx, src);
			while ((edx & 0xf) != mix) {
				if ((esi) > 0xf) {
					printk(KERN_ERR "vortex: mixdelWTD: error lifeboat overflow\n");
					return 0;
				}
				esp14 = ebx;
				ebx = edx & 0xf;
				ebp = ebx << 2;
				edx = hwread(vortex->mmio, VORTEX_MIXER_RTBASE + ebp);
				//printk(KERN_INFO "vortex: mixdelWTD: while addr=%x, val=%x\n", ebp, edx);
				esi++;
			}
			//7b30
			ebp = ebx << 2;
			if (edx & 0x10) {	/* Delete entry in between others */
				ebx = VORTEX_MIXER_RTBASE + ((edx & 0xf) << 2);
				edx = hwread(vortex->mmio, ebx);
				//7b60
				hwwrite(vortex->mmio, VORTEX_MIXER_RTBASE + ebp, edx);
				hwwrite(vortex->mmio, ebx, 0);
				//printk(KERN_INFO "vortex mixdelWTD between addr= 0x%x, val= 0x%x\n", ebp, edx);
			} else {			/* Delete last entry */
				//7b83
				if (esp14 == -1)
					hwwrite(vortex->mmio, VORTEX_MIXER_CHNBASE + (ch << 2), esp18 & 0xef);
				else {
					ebx = (0xffffffe0 & edx) | (0xf & ebx);
					hwwrite(vortex->mmio, VORTEX_MIXER_RTBASE + (esp14 << 2), ebx);
					//printk(KERN_INFO "vortex mixdelWTD last addr= 0x%x, val= 0x%x\n", esp14, ebx);
				}
				hwwrite(vortex->mmio, VORTEX_MIXER_RTBASE + ebp, 0);
				return 1;
			}
	    }
    } else {
		//printk(KERN_INFO "removed last mix\n");
		//7be0
		vortex_mixer_dis_sr(vortex, ch);
		hwwrite(vortex->mmio, ebp, 0);
    }
    return 1;
}

unsigned int vortex_mix_boost6db(unsigned char vol) {
    return (vol + 8);		/* WOW! what a complex function! */
}

void vortex_mix_rampvolume(vortex_t *vortex, int mix) {
    int ch;
    char a;
    // This function is intended for ramping down only (see vortex_disableinput()).
    for (ch = 0; ch < 0x20; ch++) {
	if (((1 << ch) & rampchs[mix]) == 0)
	    continue;
	a = hwread(vortex->mmio,
		   VORTEX_MIX_INVOL_B + (((mix << 5) + ch) << 2));
	if (a > -126) {
	    a -= 2;
	    hwwrite(vortex->mmio,
		    VORTEX_MIX_INVOL_A + (((mix << 5) + ch) << 2), a);
	    hwwrite(vortex->mmio,
		    VORTEX_MIX_INVOL_B + (((mix << 5) + ch) << 2), a);
	} else
	    vortex_mix_killinput(vortex, mix, ch);
    }
}

void vortex_mixer_init(vortex_t *vortex) {
    unsigned long addr;
    int x;

	// FIXME: get rid of this crap.
	memset(mchannels, 0, NR_MIXOUT*sizeof(int));
	memset(rampchs, 0, NR_MIXOUT*sizeof(int));

	addr = VORTEX_MIX_U0 + 0x17c;
    for (x = 0x5f; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0);
		addr -= 4;
    }
	addr = VORTEX_MIX_ENIN + 0x1fc;
    for (x = 0x7f; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0);
		addr -= 4;
    }
	addr = VORTEX_MIX_U0 + 0x17c;
    for (x = 0x5f; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0);
		addr -= 4;
    }
	addr = VORTEX_MIX_INVOL_A + 0x7fc;
    for (x = 0x1ff; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0x80);
		addr -= 4;
    }
	addr = VORTEX_MIX_VOL_A + 0x3c;
    for (x = 0xf; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0x80);
		addr -= 4;
    }
	addr = VORTEX_MIX_INVOL_B + 0x7fc;
    for (x = 0x1ff; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0x80);
		addr -= 4;
    }
	addr = VORTEX_MIX_VOL_B + 0x3c;
    for (x = 0xf; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0x80);
		addr -= 4;
    }
	addr = VORTEX_MIXER_RTBASE + (MIXER_RTBASE_SIZE-1)*4;
	for (x = (MIXER_RTBASE_SIZE-1); x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0x0);
		addr -= 4;
    }
    hwwrite(vortex->mmio, VORTEX_MIXER_SR, 0);
    /*
       call CAsp4Mix__Initialize_CAsp4HwIO____CAsp4Mixer____
       Register ISR callback for volume smooth fade out.
       Maybe this avoids clicks when press "stop" ?
     */
}

/*  SRC (CAsp4Src.s and CAsp4SrcBlock) */

void vortex_src_en_sr(vortex_t *vortex, int channel) {
    hwwrite(vortex->mmio, VORTEX_SRCBLOCK_SR, hwread(vortex->mmio, VORTEX_SRCBLOCK_SR) | (0x1 << channel));
}

void vortex_src_dis_sr(vortex_t *vortex, int channel) {
    hwwrite(vortex->mmio, VORTEX_SRCBLOCK_SR, hwread(vortex->mmio, VORTEX_SRCBLOCK_SR) & ~(0x1 << channel));
}

void vortex_src_flushbuffers(vortex_t *vortex, unsigned char src) {
    int i;

    for (i = 0x1f; i >= 0; i--)
		hwwrite(vortex->mmio, VORTEX_SRC_DATA0 + (src << 7) + (i << 2), 0);
    hwwrite(vortex->mmio, VORTEX_SRC_DATA + (src << 3), 0);
    hwwrite(vortex->mmio, VORTEX_SRC_DATA + (src << 3) + 4, 0);
}

void vortex_src_cleardrift(vortex_t *vortex, unsigned char src) {
    hwwrite(vortex->mmio, VORTEX_SRC_DRIFT0 + (src << 2), 0);
    hwwrite(vortex->mmio, VORTEX_SRC_DRIFT1 + (src << 2), 0);
    hwwrite(vortex->mmio, VORTEX_SRC_DRIFT2 + (src << 2), 1);
}

void vortex_src_slowlock(vortex_t *vortex, unsigned char src) {
    int temp;

    hwwrite(vortex->mmio, VORTEX_SRC_DRIFT2 + (src << 2), 1);
    hwwrite(vortex->mmio, VORTEX_SRC_DRIFT0 + (src << 2), 0);
    temp = hwread(vortex->mmio, VORTEX_SRC_U0 + (src << 2));
    if (temp & 0x200)
		hwwrite(vortex->mmio, VORTEX_SRC_U0 + (src << 2), temp & ~0x200L);
}

void vortex_src_set_throttlesource(vortex_t *vortex, unsigned char src, int en) {
    int temp;

    temp = hwread(vortex->mmio, VORTEX_SRC_SOURCE);
    if (en)
		temp |= 1 << src;
    else
		temp &= ~(1 << src);
    hwwrite(vortex->mmio, VORTEX_SRC_SOURCE, temp);
}

int  vortex_src_persist_convratio(vortex_t *vortex, unsigned char src, int ratio) {
    int temp, lifeboat = 0;

    do {
		hwwrite(vortex->mmio, VORTEX_SRC_CONVRATIO + (src << 2), ratio);
		temp = hwread(vortex->mmio, VORTEX_SRC_CONVRATIO + (src << 2));
		if ((++lifeboat) > 0x9) {
            printk(KERN_ERR "Vortex: Src cvr fail\n");
			break;
        }
    } while (temp != ratio);
    return temp;
}

void vortex_src_change_convratio(vortex_t *vortex, unsigned char src, int ratio) {
    int temp, a;

    if ((ratio & 0x10000) && (ratio != 0x10000)) {
		if (ratio & 0x3fff)
			a = (0x11 - ((ratio >> 0xe) & 0x3)) - 1;
		else
			a = (0x11 - ((ratio >> 0xe) & 0x3)) - 2;
	} else
		a = 0xc;
    temp = hwread(vortex->mmio, VORTEX_SRC_U0 + (src << 2));
    if (((temp >> 4) & 0xf) != a)
		hwwrite(vortex->mmio, VORTEX_SRC_U0 + (src << 2), (temp & 0xf) | ((a & 0xf) << 4));

    vortex_src_persist_convratio(vortex, src, ratio);
}

int  vortex_src_checkratio(vortex_t *vortex, unsigned char src, unsigned int desired_ratio) {
    int hw_ratio, lifeboat = 0;

    hw_ratio = hwread(vortex->mmio, VORTEX_SRC_CONVRATIO + (src << 2));
    
    while (hw_ratio != desired_ratio) {
		hwwrite(vortex->mmio, VORTEX_SRC_CONVRATIO + (src << 2), desired_ratio);
		
		if ((lifeboat++) > 15){
			printk(KERN_ERR "Vortex: could not set src-%d from %d to %d\n", src, hw_ratio, desired_ratio);
			break;
		}
    }
    
    return hw_ratio;
}

void vortex_src_setupchannel(vortex_t *card, unsigned char src, unsigned int cvr,
    unsigned int b, int c, int d, int dirplay, int f, unsigned int g, int thsource) {
    // noplayback: d=2,4,7,0xa,0xb when using first 2 src's.
	// c: enables pitch sweep.
	// looks like g is c related. Maybe g is a sweep parameter ?
	// g = cvr
	// dirplay: 0 = recording, 1 = playback
	// d = src hw index.
		
    int esi, ebp=0, esp10;

    vortex_src_flushbuffers(card, src);

    if (c) {
		if ((g & 0x10000) && (g != 0x10000)) {
			g = 0;
			esi = 0x7;
		} else {
			if ((((short)g) < 0) && (g != 0x8000)) {
				g = 0;
				esi = 0x8;
			} else {
				g = 1;
				esi = 0xc;
			}
		}
    } else {
		if ((cvr & 0x10000) && (cvr != 0x10000)) {
			g = 0; /*ebx = 0*/
			esi = 0x11 - ((cvr >> 0xe) & 7);
			if (cvr & 0x3fff)
				esi -= 1;
			else
				esi -= 2;
		} else {
			g = 1;
			esi = 0xc;
		}
    }
    vortex_src_cleardrift(card, src);
    vortex_src_set_throttlesource(card, src, thsource);

    if ((dirplay == 0)&&(c == 0)) {
        if (g)
            esp10 = 0xf;
        else
            esp10 = 0xc;
        ebp = 0;
    } else {
    	if (g)
		    ebp = 0xf;
		else
    		ebp = 0xc;
		esp10 = 0;       
    }        
	hwwrite(card->mmio, VORTEX_SRC_U0 + (src << 2), (f << 0x9) | (c << 0x8) | ((esi&0xf)<<4) | d);
    /* 0xc0   esi=0xc c=f=0 d=0*/
	vortex_src_persist_convratio(card, src, cvr);
    hwwrite(card->mmio, VORTEX_SRC_U1 + (src << 2), b & 0xffff);
	/* 0   b=0 */
    hwwrite(card->mmio, VORTEX_SRC_U3 + (src << 2), (g << 0x11) | (dirplay << 0x10) | (ebp << 0x8) | esp10);
	/* 0x30f00 e=g=1 esp10=0 ebp=f */
	//printk(KERN_INFO "vortex: SRC %d, d=0x%x, esi=0x%x, esp10=0x%x, ebp=0x%x\n", src, d, esi, esp10, ebp);
}

void vortex_srcblock_init(vortex_t *vortex) {
    unsigned long addr;
    int x;
    hwwrite(vortex->mmio, VORTEX_SRC_SOURCE+4, 0x1ff);
    /*
    for (x=0; x<0x10; x++) {
		vortex_src_init(&vortex_src[x], x);
    }
    */
    //addr = 0xcc3c;
	//addr = 0x26c3c;
	addr = VORTEX_SRC_RTBASE + 0x3c;
    for (x = 0xf; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0);
		addr -= 4;
    }
    //addr = 0xcc94;
	//addr = 0x26c94;
	addr = VORTEX_SRC_CHNBASE + 0x54;
    for (x = 0x15; x >= 0; x--) {
		hwwrite(vortex->mmio, addr, 0);
		addr -= 4;
    }
}

int  vortex_src_addWTD(vortex_t *vortex, unsigned char src, unsigned char ch) {
    int temp, lifeboat = 0, prev;
	// esp13 = src

    temp = hwread(vortex->mmio, VORTEX_SRCBLOCK_SR);
    if ((temp & (1 << ch)) == 0) {
    	hwwrite(vortex->mmio, VORTEX_SRC_CHNBASE + (ch << 2), src);
    	vortex_src_en_sr(vortex, ch);
    	return 1;
    }
    prev = VORTEX_SRC_CHNBASE + (ch << 2); /*ebp*/
    temp = hwread(vortex->mmio, prev);
    //while (temp & NR_SRC) {
	while (temp & 0x10) {
	    prev = VORTEX_SRC_RTBASE + ((temp & 0xf) << 2); /*esp12*/
		//prev = VORTEX_SRC_RTBASE + ((temp & (NR_SRC-1)) << 2); /*esp12*/
	    temp = hwread(vortex->mmio, prev);
		//printk(KERN_INFO "vortex: srcAddWTD: while addr=%x, val=%x\n", prev, temp);
	    if ((++lifeboat) > 0xf) {
    	    printk(KERN_ERR "vortex_src_addWTD: lifeboat overflow\n");
	        return 0;
	    }
    }
    hwwrite(vortex->mmio, VORTEX_SRC_RTBASE + ((temp & 0xf) << 2), src);
    //hwwrite(vortex->mmio, prev, (temp & (NR_SRC-1)) | NR_SRC);
	hwwrite(vortex->mmio, prev, (temp & 0xf) | 0x10);
    return 1;
}

int  vortex_src_delWTD(vortex_t *vortex, unsigned char src, unsigned char ch) {
	int esp14=-1, esp18, eax, ebx, edx, ebp, esi=0;
	//int esp1f=edi(while)=src, esp10=ch;

    eax = hwread(vortex->mmio, VORTEX_SRCBLOCK_SR);
    if (((1 << ch) & eax) == 0) {
		printk(KERN_ERR "src alarm\n");
		return 0;
	}
	ebp = VORTEX_SRC_CHNBASE + (ch << 2);
    esp18 = hwread(vortex->mmio, ebp);
    if (esp18 & 0x10) {
		ebx = (esp18 & 0xf);
		if (src == ebx) {
			ebx = VORTEX_SRC_RTBASE + (src << 2);
			edx = hwread(vortex->mmio, ebx);
			//7b60
			hwwrite(vortex->mmio, ebp, edx);
			hwwrite(vortex->mmio, ebx, 0);
		} else {
			//7ad3
			edx = hwread(vortex->mmio, VORTEX_SRC_RTBASE + (ebx << 2));
			//printk(KERN_INFO "vortex: srcdelWTD: 1 addr=%x, val=%x, src=%x\n", ebx, edx, src);
			while ((edx & 0xf) != src) {
				if ((esi) > 0xf) {
					printk("vortex: srcdelWTD: error, lifeboat overflow\n");
					return 0;
				}
				esp14 = ebx;
				ebx = edx & 0xf;
				ebp = ebx << 2;
				edx = hwread(vortex->mmio, VORTEX_SRC_RTBASE + ebp);
				//printk(KERN_INFO "vortex: srcdelWTD: while addr=%x, val=%x\n", ebp, edx);
				esi++;
			}
			//7b30
			ebp = ebx << 2;
			if (edx & 0x10) {	/* Delete entry in between others */
				ebx = VORTEX_SRC_RTBASE + ((edx & 0xf) << 2);
				edx = hwread(vortex->mmio, ebx);
				//7b60
				hwwrite(vortex->mmio, VORTEX_SRC_RTBASE + ebp, edx);
				hwwrite(vortex->mmio, ebx, 0);
				//printk(KERN_INFO "vortex srcdelWTD between addr= 0x%x, val= 0x%x\n", ebp, edx);
			} else {			/* Delete last entry */
				//7b83
				if (esp14 == -1)
					hwwrite(vortex->mmio, VORTEX_SRC_CHNBASE + (ch << 2), esp18 & 0xef);
				else {
					ebx = (0xffffffe0 & edx) | (0xf & ebx);
					hwwrite(vortex->mmio, VORTEX_SRC_RTBASE + (esp14 << 2), ebx);
					//printk(KERN_INFO"vortex srcdelWTD last addr= 0x%x, val= 0x%x\n", esp14, ebx);
				}
				hwwrite(vortex->mmio, VORTEX_SRC_RTBASE + ebp, 0);
				return 1;
			}
	    }
    } else {
		//7be0
		vortex_src_dis_sr(vortex, ch);
		hwwrite(vortex->mmio, ebp, 0);
    }
    return 1;
}

 /*FIFO*/

void vortex_fifo_clearadbdata(vortex_t *vortex, int fifo, int x) {
	for (x--; x >= 0; x--)
		hwwrite(vortex->mmio, VORTEX_FIFO_ADBDATA + (((fifo << FIFO_SIZE_BITS) + x) << 2), 0);
}

void vortex_fifo_clearwtdata(vortex_t *vortex, int fifo, int x) {
	if (x < 1)
		return;
	for (x--; x >= 0; x--)
		hwwrite(vortex->mmio, VORTEX_FIFO_WTDATA + (((fifo << FIFO_SIZE_BITS) + x) << 2), 0);
}

void vortex_fifo_init(vortex_t *vortex) {
    int x;
    unsigned long addr;

    /* ADB DMA channels fifos. */
	addr = VORTEX_FIFO_ADBCTRL + ((NR_ADB-1)*4);
    for (x = NR_ADB-1; x >= 0; x--) {
	    hwwrite(vortex->mmio, addr, (FIFO_U0 | FIFO_U1));
	    if (hwread(vortex->mmio, addr) != (FIFO_U0 | FIFO_U1))
	        printk(KERN_ERR "bad adb fifo reset!");
	    vortex_fifo_clearadbdata(vortex, x, FIFO_SIZE);
	    addr -= 4;
    }

#ifndef CHIP_AU8810
    /* WT DMA channels fifos. */
	addr = VORTEX_FIFO_WTCTRL + ((NR_WT-1)*4);
    for (x = NR_WT-1; x >= 0; x--) {
	    hwwrite(vortex->mmio, addr, FIFO_U0);
	    if (hwread(vortex->mmio, addr) != FIFO_U0)
    	    printk(KERN_ERR "bad wt fifo reset (0x%08lx, 0x%08x)!\n", addr, hwread(vortex->mmio, addr));
	    vortex_fifo_clearwtdata(vortex, x, FIFO_SIZE);
	    addr -= 4;
    }
#endif
    /* below is weird stuff... */
#ifndef CHIP_AU8820
    hwwrite(vortex->mmio, 0xf8c0, 0xd03); //0x0843 0xd6b
#else
	hwwrite(vortex->mmio, 0x17000, 0x61);
	hwwrite(vortex->mmio, 0x17004, 0x61);
	hwwrite(vortex->mmio, 0x17008, 0x61);
#endif
}

void vortex_fifo_adbinitialize(vortex_t *vortex, int fifo, int j) {
    vortex_fifo_clearadbdata(vortex, fifo, FIFO_SIZE);
#ifdef CHIP_AU8820
    hwwrite(vortex->mmio, VORTEX_FIFO_ADBCTRL + (fifo << 2), (FIFO_U1 | ((j & FIFO_MASK) << 0xb)));
#else
	hwwrite(vortex->mmio, VORTEX_FIFO_ADBCTRL + (fifo << 2), (FIFO_U1 | ((j & FIFO_MASK) << 0xc)));
#endif
}

void vortex_fifo_wtinitialize(vortex_t *vortex, int fifo, int j) {
    vortex_fifo_clearwtdata(vortex, fifo, FIFO_SIZE);
#ifdef CHIP_AU8820
    hwwrite(vortex->mmio, VORTEX_FIFO_WTCTRL + (fifo << 2), (FIFO_U1 | ((j & FIFO_MASK) << 0xb)));
#else
	hwwrite(vortex->mmio, VORTEX_FIFO_WTCTRL + (fifo << 2), (FIFO_U1 | ((j & FIFO_MASK) << 0xc)));
#endif
}

void vortex_fifo_setadbvalid(vortex_t *vortex, int fifo, int en) {
    hwwrite(vortex->mmio, VORTEX_FIFO_ADBCTRL + (fifo << 2), (hwread(vortex->mmio,
		VORTEX_FIFO_ADBCTRL + (fifo << 2)) & 0xffffffef) | ((1 & en) << 4) | FIFO_U1);
}

void vortex_fifo_setwtvalid(vortex_t *vortex, int fifo, int en) {
    hwwrite(vortex->mmio, VORTEX_FIFO_WTCTRL + (fifo << 2), (hwread(vortex->mmio,
		VORTEX_FIFO_WTCTRL + (fifo << 2)) & 0xffffffef) | ((en & 1) << 4) | FIFO_U1);
}

void vortex_fifo_setadbctrl(vortex_t *vortex, int fifo, int b, int priority, int empty, int valid, int f) {
    int temp, lifeboat=0;
    //int this_8[NR_ADB] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; /* position */
	int	this_4=0x2;
	/* f seems priority related.
	 * CAsp4AdbDma::SetPriority is the only place that calls SetAdbCtrl with f set to 1
	 * every where else it is set to 0. It seems, however, that CAsp4AdbDma::SetPriority
	 * is never called, thus the f related bits remain a mystery for now.
	 */
    do {
		temp = hwread(vortex->mmio, VORTEX_FIFO_ADBCTRL + (fifo << 2));
		if (lifeboat++ > 0xbb8) {
			printk(KERN_ERR "Vortex: vortex_fifo_setadbctrl fail\n");
			break;
		}
    } while (temp & FIFO_RDONLY);

	// AU8830 semes to take some special care about fifo content (data).
	// But i'm just to lazy to translate that :)
    if (valid) {
		if ((temp & FIFO_VALID) == 0) {
			//this_8[fifo] = 0;
			vortex_fifo_clearadbdata(vortex, fifo, FIFO_SIZE); // this_4
#ifdef CHIP_AU8820
			temp = (this_4 & 0x1f) << 0xb;
#else
			temp = (this_4 & 0x3f) << 0xc;
#endif
			temp = (temp & 0xfffffffd) | ((b & 1) << 1);
			temp = (temp & 0xfffffff3) | ((priority & 3) << 2);
			temp = (temp & 0xffffffef) | ((valid & 1) << 4);
			temp |= FIFO_U1;
			temp = (temp & 0xffffffdf) | ((empty & 1) << 5);
#ifdef CHIP_AU8820
			temp = (temp & 0xfffbffff) | ((f & 1) << 0x12);
#endif
#ifdef CHIP_AU8830
			temp = (temp & 0xf7ffffff) | ((f & 1) << 0x1b);
			temp = (temp & 0xefffffff) | ((f & 1) << 0x1c);
#endif
#ifdef CHIP_AU8810
			temp = (temp & 0xfeffffff) | ((f & 1) << 0x18);
			temp = (temp & 0xfdffffff) | ((f & 1) << 0x19);
#endif
		}
    } else {
		if (temp & FIFO_VALID) {
#ifdef CHIP_AU8820
			temp = ((f & 1) << 0x12) | (temp & 0xfffbffef);
#endif
#ifdef CHIP_AU8830
			temp = ((f & 1) << 0x1b) | (temp & 0xe7ffffef) | FIFO_BITS;
#endif
#ifdef CHIP_AU8810
			temp = ((f & 1) << 0x1b) | (temp & 0xfcffffef) | FIFO_BITS;
#endif			
		} else
			/*if (this_8[fifo])*/ vortex_fifo_clearadbdata(vortex, fifo, FIFO_SIZE);
	}
	hwwrite(vortex->mmio, VORTEX_FIFO_ADBCTRL + (fifo << 2), temp);
	hwread(vortex->mmio, VORTEX_FIFO_ADBCTRL + (fifo << 2));
}

void vortex_fifo_setwtctrl(vortex_t *vortex, int fifo, int b, int priority, int empty, int valid, int f) {
	int temp=0;
	int this_4=2;

	if (valid) {
		vortex_fifo_clearwtdata(vortex, fifo, 0x20);
		temp = (this_4 & 0x1f) << 0xb;
	}
	temp = (temp & 0xfffffffd) | ((b & 1) << 1);
	temp = (temp & 0xfffdffff) | ((f & 1) << 0x11);
	temp = (temp & 0xfffffff3) | ((priority & 3) << 2);
	temp = (temp & 0xffffffef) | ((valid & 1) << 4);
	temp = (temp & 0xffffffdf) | ((empty & 1) << 5);
#ifdef FIFO_BITS
	temp |= FIFO_BITS;
#endif
	hwwrite(vortex->mmio, VORTEX_FIFO_WTCTRL + (fifo << 2), temp);
}

/* ADBDMA */

void vortex_adbdma_init(vortex_t *vortex) {
}

void vortex_adbdma_setupbuffer(vortex_t *vortex, int adbdma, int sb, u32 addr, u32 size, int sb_next, int c) {
    // if "c" is 0, then DMA stops when reaching that buffer.

    int offset, shift, mask;
    stream_t *dma = &vortex->dma_adb[adbdma];
    int *cfg;

    if (sb_next == -1) {
        sb_next = sb;
        if (dma->dma_ctrl & IE_MASK)
            c = 0;
    }

    if ((sb >= 0)&&(sb < 4)) {
        if (sb & 1) {
            // subbuffer 1 and 3
            shift = 0x0;
            mask = 0xf8fff000;
        } else {
            // subbuffer 0 and 2
            shift = 0xc;
            mask = 0x8f000fff;
        }
        if (sb & 2) {
            // subbuffer 2 and 3
            cfg = &dma->cfg1;
            offset = 4;
        } else {
            // subbuffer 0 and 1
            cfg = &dma->cfg0;
            offset = 0;
        }
        *cfg = (*cfg & (mask|0xffffff)) | ((((sb_next & 3)<<4) | ((c&1)<<6))<<24) | 0x80;
        if (size)
           *cfg = (*cfg & (mask|0xff000000)) | (((size-1) & 0xfff) << shift);
        hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFCFG0 + offset + (adbdma << 3), *cfg);
    }
    hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFBASE + (((adbdma<<2)+sb)<<2), addr);
}

void vortex_adbdma_setfirstbuffer(vortex_t *vortex, int adbdma) {
    stream_t *dma = &vortex->dma_adb[adbdma];

    hwwrite(vortex->mmio, VORTEX_ADBDMA_CTRL + (adbdma << 2), dma->dma_ctrl);
}

void vortex_adbdma_setstartbuffer(vortex_t *vortex, int adbdma, int sb) {
    stream_t *dma = &vortex->dma_adb[adbdma];
	hwwrite(vortex->mmio, VORTEX_ADBDMA_START + (adbdma << 2), (sb << ((NR_ADB-adbdma)*2)));
	dma->period_real = dma->period_virt = sb;
}

void vortex_adbdma_setbuffers(vortex_t *vortex, int adbdma, unsigned int addr, int size, int count) {
	stream_t *dma = &vortex->dma_adb[adbdma];

	dma->period_bytes = size;
	dma->nr_periods = count;
	dma->buf_addr = addr;

	// Adb Buffers. Simple linear buffer.
	hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFBASE + (adbdma << 4), addr);
	hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFBASE + 0x4 + (adbdma << 4), addr + size);
	hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFBASE + 0x8 + (adbdma << 4), addr + (size*2));
	hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFBASE + 0xc + (adbdma << 4), addr + (size*3));
	
	size--;
	switch (count) {
	case 1:
		dma->cfg0 = 0x80000000 | 0x40000000 | 0x00000000 | (size << 0xc);
		dma->cfg1 = 0x00000000 | 0x00000000 | 0x00000000;
		break;
	case 2:
		dma->cfg0 = 0x88000000 | 0x44000000 | 0x10000000 | (size << 0xc) | size;
		dma->cfg1 = 0x00000000 | 0x00000000 | 0x00000000;
		break;
	case 3:
		dma->cfg0 = 0x88000000 | 0x44000000 | 0x12000000 | (size << 0xc) | size;
		dma->cfg1 = 0x80000000 | 0x40000000 | 0x00000000 | (size << 0xc);
		break;
	case 4:
	default:
		dma->cfg0 = 0x88000000 | 0x44000000 | 0x12000000 | (size << 0xc) | size;
		dma->cfg1 = 0x88000000 | 0x44000000 | 0x30000000 | (size << 0xc) | size;
		break;
	}	
	hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFCFG0 + (adbdma << 3), dma->cfg0);
	hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFCFG1 + (adbdma << 3), dma->cfg1);

	vortex_adbdma_setfirstbuffer(vortex, adbdma);
	vortex_adbdma_setstartbuffer(vortex, adbdma, 0);
}

void vortex_adbdma_setmode(vortex_t *vortex, int adbdma, int ie, int dir, int fmt, int d, unsigned long offset) {
    stream_t *dma = &vortex->dma_adb[adbdma];
	
    dma->dma_unknown = d;
    dma->dma_ctrl = ((offset & OFFSET_MASK) | (dma->dma_ctrl & ~OFFSET_MASK));
    /* Enable PCMOUT interrupts. */
    dma->dma_ctrl = (dma->dma_ctrl & ~IE_MASK) | ((ie << IE_SHIFT) & IE_MASK);

    dma->dma_ctrl = (dma->dma_ctrl & ~U_MASK) | ((dir << U_SHIFT) & U_MASK);
    dma->dma_ctrl = (dma->dma_ctrl & ~FMT_MASK) | ((fmt << FMT_SHIFT) & FMT_MASK);

    hwwrite(vortex->mmio, VORTEX_ADBDMA_CTRL + (adbdma << 2), dma->dma_ctrl);
	hwread(vortex->mmio, VORTEX_ADBDMA_CTRL + (adbdma << 2));
}

void vortex_adbdma_bufshift(vortex_t *vortex, int adbdma) {
	stream_t *dma = &vortex->dma_adb[adbdma];
	int page, p, pp, delta, i;
	
 	page = (hwread(vortex->mmio, VORTEX_ADBDMA_STAT + (adbdma << 2)) & ADB_SUBBUF_MASK) >> ADB_SUBBUF_SHIFT;
	if (dma->nr_periods >= 4)
		delta = (page - dma->period_real) & 3;
	else {
		delta = (page - dma->period_real);
		if (delta < 0)
			delta += dma->nr_periods;
	}
	if (delta == 0)
		return;

	/* refresh hw page table */
	if (dma->nr_periods > 4) {
		for (i=0 ; i < delta; i++) {
			/* p: audio buffer page index */
			p = dma->period_virt + i + 4;
			if (p >= dma->nr_periods)
				p -= dma->nr_periods;
			/* pp: hardware DMA page index. */
			pp = dma->period_real + i;
			if (pp >= 4)
				pp -= 4;
			hwwrite(vortex->mmio, VORTEX_ADBDMA_BUFBASE+(((adbdma << 2)+pp) << 2), dma->buf_addr+((dma->period_bytes)*p));
			/* Force write thru cache. */
			hwread(vortex->mmio, VORTEX_ADBDMA_BUFBASE + (((adbdma << 2)+pp) << 2));
		}
	}
	dma->period_virt += delta;
	dma->period_real = page;
	if (dma->period_virt >= dma->nr_periods)
		dma->period_virt -= dma->nr_periods;
	if ((delta != 1) || (delta < 0))
		printk(KERN_INFO "vortex: % d virt=0x%d, real=0x%x, delta = %d\n", adbdma, dma->period_virt, dma->period_real, delta);
}

void vortex_adbdma_setctrl(vortex_t *vortex, int a, int b, int c, int d, int e, int f) {
    //FIXME
}

void vortex_adbdma_getposition(vortex_t *vortex, int adbdma, int *subbuf, int *pos) {
    int temp, edi, eax, ecx, esp10, edx;
    stream_t *dma = &vortex->dma_adb[adbdma];

    // dma->cfg0: this_7c,  dma->cfg1: this_80.

    // Looks like several hardware bug workarounds are in here.
    // The most weird function. What a nightmare.
    do {
        // Potential lockup, no lifeboat.
	    temp = hwread(vortex->mmio, VORTEX_ADBDMA_STAT + (adbdma << 2));
        if (temp == -1) {
		    // 3ae1
            //*subbuf = this_88;
            //*pos = this_84;
		    return;
        }
        esp10 = adbdma;
		if ((esp10 == ((temp >> 0x13) & 0xf)) && ((char)((temp >> 0x10) & 0xff) < 0))
            continue;
        edx = temp >> 0x18;
        if ((edx & 0xf) != adbdma)
		    break;
    } while (edx | 0x10);

    ecx = hwread(vortex->mmio, VORTEX_FIFO_ADBCTRL + (adbdma << 2));
    *subbuf = (ecx >> 0xc) & 3;

    switch (*subbuf) {
        case 0:
            edi = (dma->cfg0 >> 0xc) & 0xfff;
            eax = (dma->cfg0 >> 24) & 0xff;
            break;
        case 1:
            edi = dma->cfg0 & 0xfff;
            eax = (dma->cfg0 >> 24) & 0xff;
            break;
        case 2:
            edi = (dma->cfg1 >> 0xc) & 0xfff;
            eax = (dma->cfg1 >> 24) & 0xff;
            break;
        case 3:
            edi = dma->cfg1 & 0xfff;
            eax = (dma->cfg1 >> 24) & 0xff;
            break;
        default:
            return;
    }
    edi++;
    if (dma->fifo_enabled == 0)
	    *subbuf = 0;
    else {
        esp10 = dma->dma_unknown;
        temp = (temp & 0xfff83fff) | ((((temp >> 0xe) & 0x1f) & ~esp10) << 0xe);
        if (dma->dma_ctrl & 0x2000) {
            if (esp10) {
                if ((dma->dma_ctrl & 0x3c000) == 0x20000) /* if stereo */
                    eax = (0x1c - ((temp >> 0xe) & 0x1f))*2;
                else
                    eax = 0x1c - ((temp >> 0xe) & 0x1f);
            } else {
                if ((dma->dma_ctrl & 0x3c000) == 0x20000) /* if stereo */
                    eax = ((~(temp >> 0xe)) & 0x1f)*2;
                else
                    eax = (~(temp >> 0xe)) & 0x1f;
            }
            if (((ecx & 0x10) == 0) && (dma->fifo_status != FIFO_PAUSE))
                eax = edi - eax;
            else
                eax = (temp & 0xfff) - eax;
            if (edi > eax)
                eax = edi;
        } else {
            // 3a87
            if ((dma->dma_ctrl & 0x3c000) == 0x20000) /* if stereo */
                eax = (temp >>  0xd) & 0x3e;
            else
                eax = (temp >> 0xe) & 0x1f;
        }
        // 3aa8
        edi--;
        if ((ecx & 0x10) == 0) {
            if (dma->fifo_status == FIFO_PAUSE) {
                edx = (temp & 0xfff) + eax;
                eax = edi + 1;
                if (eax < edx)
                    eax = edx;
            } else
                eax = ((ecx >> 5) & 1) + edi;
        } else {
             // 3ad3
             eax += temp & 0xfff;
             if (edi >= eax)
                 eax = edi; // 3b00
        }
    }
    // 3b02
    *subbuf = eax;
    // 3b08
    //this_84 = *pos;
    //this_88 = *subbuf;
}

int  vortex_adbdma_getcursubuffer(vortex_t *vortex, int adbdma) {
    return((hwread(vortex->mmio, VORTEX_ADBDMA_STAT + (adbdma << 2)) & ADB_SUBBUF_MASK) >> ADB_SUBBUF_SHIFT);
}

int inline vortex_adbdma_getlinearpos(vortex_t *vortex, int adbdma) {
    stream_t *dma = &vortex->dma_adb[adbdma];
    int temp;

    temp = hwread(vortex->mmio, VORTEX_ADBDMA_STAT + (adbdma << 2));
 	temp = (dma->period_virt * dma->period_bytes) + (temp & POS_MASK);
    return (temp);
}

void vortex_adbdma_startfifo(vortex_t *vortex, int adbdma) {
    int this_8=0/*empty*/, this_4=0/*priority*/;
	stream_t *dma = &vortex->dma_adb[adbdma];

    switch (dma->fifo_status) {
        case FIFO_START:
            vortex_fifo_setadbvalid(vortex, adbdma, dma->fifo_enabled ? 0xff:0);
            break;
        case FIFO_STOP:
			this_8 = 1;
            hwwrite(vortex->mmio, VORTEX_ADBDMA_CTRL + (adbdma << 2), dma->dma_ctrl);
            vortex_fifo_setadbctrl(vortex, adbdma, dma->dma_unknown, this_4,
				this_8, dma->fifo_enabled ? 0xff:0, 0);
            break;
        case FIFO_PAUSE:
            vortex_fifo_setadbctrl(vortex, adbdma, dma->dma_unknown, this_4,
				this_8, dma->fifo_enabled ? 0xff:0, 0);
            break;
    }
    dma->fifo_status = FIFO_START;
}

void vortex_adbdma_resumefifo(vortex_t *vortex, int adbdma) {
	stream_t *dma = &vortex->dma_adb[adbdma];

    int this_8=1, this_4=0;
    switch (dma->fifo_status) {
        case FIFO_STOP:
            hwwrite(vortex->mmio, VORTEX_ADBDMA_CTRL + (adbdma << 2), dma->dma_ctrl);
            vortex_fifo_setadbctrl(vortex, adbdma, dma->dma_unknown, this_4, this_8,
				dma->fifo_enabled ? 0xff:0, 0);
            break;
        case FIFO_PAUSE:
            vortex_fifo_setadbctrl(vortex, adbdma, dma->dma_unknown, this_4, this_8,
				dma->fifo_enabled ? 0xff:0, 0);
            break;
    }
   	dma->fifo_status = FIFO_START;
}

void vortex_adbdma_pausefifo(vortex_t *vortex, int adbdma) {
	stream_t *dma = &vortex->dma_adb[adbdma];

    int this_8=0, this_4=0;
    switch (dma->fifo_status) {
        case FIFO_START:
            vortex_fifo_setadbctrl(vortex, adbdma, dma->dma_unknown, this_4, this_8, 0, 0);
            break;
        case FIFO_STOP:
            hwwrite(vortex->mmio, VORTEX_ADBDMA_CTRL + (adbdma << 2), dma->dma_ctrl);
            vortex_fifo_setadbctrl(vortex, adbdma, dma->dma_unknown, this_4, this_8, 0, 0);
            break;
    }
    dma->fifo_status = FIFO_PAUSE;
}

void vortex_adbdma_stopfifo(vortex_t *vortex, int adbdma) {
	stream_t *dma = &vortex->dma_adb[adbdma];

    int this_4=0, this_8=0;
    if (dma->fifo_status == FIFO_START)
        vortex_fifo_setadbctrl(vortex, adbdma, dma->dma_unknown, this_4, this_8, 0, 0);
    else if (dma->fifo_status == FIFO_STOP)
        return;
    dma->fifo_status = FIFO_STOP;
    dma->fifo_enabled = 0;
}

/* WTDMA */
#ifndef CHIP_AU8810
void vortex_wtdma_init(vortex_t *vortex) {
}

void vortex_wtdma_setupbuffer(vortex_t *vortex, int wtdma, int sb, u32 addr, u32 size, int sb_next, int c) {
    int offset, shift, mask;
    stream_t *dma = &vortex->dma_wt[wtdma];
    int *cfg;

    if (sb_next == -1) {
        sb_next = sb;
        if (dma->dma_ctrl & IE_MASK)
            c = 0;
    }

    if ((sb >= 0)&&(sb < 4)) {
        if (sb & 1) {
            /* subbuffer 1 and 3 */
            shift = 0x0;
            mask = 0xf8fff000;
        } else {
            /* subbuffer 0 and 2 */
            shift = 0xc;
            mask = 0x8f000fff;
        }
        if (sb & 2) {
            /* subbuffer 2 and 3 */
            cfg = &dma->cfg1;
            offset = 4;
        } else {
            /* subbuffer 0 and 1 */
            cfg = &dma->cfg0;
            offset = 0;
        }
        *cfg = (*cfg & (mask|0xffffff)) | ((((sb_next & 3)<<4) | ((c&1)<<6))<<24) | 0x80;
        if (size)
           *cfg = (*cfg & (mask|0xff000000)) | (((size-1) & 0xfff) << shift);
        hwwrite(vortex->mmio, VORTEX_WTDMA_BUFCFG0 + offset + (wtdma << 3), *cfg);
    }
    hwwrite(vortex->mmio, VORTEX_WTDMA_BUFBASE + (((wtdma<<2)+sb)<<2), addr);
}

void vortex_wtdma_setfirstbuffer(vortex_t *vortex, int wtdma) {
    //int this_7c=dma_ctrl;
    stream_t *dma = &vortex->dma_wt[wtdma];

    hwwrite(vortex->mmio, VORTEX_WTDMA_CTRL + (wtdma << 2), dma->dma_ctrl);
}

void vortex_wtdma_setstartbuffer(vortex_t *vortex, int wtdma, int sb) {
	stream_t *dma = &vortex->dma_wt[wtdma];
    hwwrite(vortex->mmio, VORTEX_WTDMA_START + (wtdma << 2), (sb << ((~wtdma)&0xf)*2));
	dma->period_real = dma->period_virt = sb;
}

void vortex_wtdma_setbuffers(vortex_t *vortex, int wtdma, unsigned int addr, int size, int count) {
    stream_t *dma = &vortex->dma_wt[wtdma];

	/* Wt buffer. */
	hwwrite(vortex->mmio, VORTEX_WTDMA_BUFBASE + (wtdma << 4), addr);
	hwwrite(vortex->mmio, VORTEX_WTDMA_BUFBASE + 0x4 + (wtdma << 4), addr + size);
	hwwrite(vortex->mmio, VORTEX_WTDMA_BUFBASE + 0x8 + (wtdma << 4), addr + (size*2));
	hwwrite(vortex->mmio, VORTEX_WTDMA_BUFBASE + 0xc + (wtdma << 4), addr + (size*3));

	switch (count) {
        	case 1:
			dma->cfg0 = 0xce000000;
			dma->cfg1 = 0xfc000000;
			break;
		case 2:
			dma->cfg0 = 0xdc000000;
			dma->cfg1 = 0xfc000000;
			break;
		case 3:
			dma->cfg0 = 0xde000000;
			dma->cfg1 = 0xcc000000;
			break;
		case 4:
		default:
			dma->cfg0 = 0xde000000;
			dma->cfg1 = 0xfc000000;
			break;
	}
	size--;
	dma->cfg0 |= (size << 0xc) | size;
	dma->cfg1 |= (size << 0xc) | size;
	//printk(KERN_INFO "vortex: wt buffer cfg: 0x%8x  0x%8x\n", dma->cfg0, dma->cfg1);
	hwwrite(vortex->mmio, VORTEX_WTDMA_BUFCFG0 + (wtdma << 3), dma->cfg0);
	hwwrite(vortex->mmio, VORTEX_WTDMA_BUFCFG1 + (wtdma << 3), dma->cfg1);

	vortex_wtdma_setfirstbuffer(vortex, wtdma);
	vortex_wtdma_setstartbuffer(vortex, wtdma, 0);
}

void vortex_wtdma_setmode(vortex_t *vortex, int wtdma, int ie, int dir, int fmt, int d, unsigned long offset) {
	stream_t *dma = &vortex->dma_wt[wtdma];

    dma->dma_unknown = d;
    dma->dma_ctrl = ((offset & OFFSET_MASK) | (dma->dma_ctrl & ~OFFSET_MASK));
    /* PCMOUT interrupt */
    dma->dma_ctrl = (dma->dma_ctrl & ~IE_MASK) | ((ie << IE_SHIFT) & IE_MASK);

    dma->dma_ctrl = (dma->dma_ctrl & U_MASK) | ((dir << U_SHIFT) & U_MASK);
    dma->dma_ctrl = (dma->dma_ctrl & FMT_MASK) | ((fmt << FMT_SHIFT) & FMT_MASK);

    hwwrite(vortex->mmio, VORTEX_WTDMA_CTRL + (wtdma << 2), dma->dma_ctrl);
}

void vortex_wtdma_bufshift(vortex_t *vortex, int wtdma) {
	stream_t *dma = &vortex->dma_wt[wtdma];
	int page, p, delta, i;

	page = (hwread(vortex->mmio, VORTEX_WTDMA_STAT + (wtdma << 2)) >> 0xc) & 0x3;
	delta = (page - dma->period_real) & 3;

	/* refresh hw page table */
	if (dma->nr_periods > 4) {
		for (i=0 ; i < delta; i++) {
			p = dma->period_virt + i;
			if (p >= dma->nr_periods)
				p -= dma->nr_periods;
			hwwrite(vortex->mmio, VORTEX_WTDMA_BUFBASE+(((wtdma << 2)+(p & 3)) << 2), dma->buf_addr + ((dma->period_bytes)*p));
		}
	}
	dma->period_virt += delta;
	if (dma->period_virt >= dma->nr_periods)
		dma->period_virt -= dma->nr_periods;
	dma->period_real = page;

	if (delta != 1)
		printk(KERN_WARNING "vortex: wt page = %d, delta = %d\n", dma->period_virt, delta);
}

void vortex_wtdma_getposition(vortex_t *vortex, int wtdma, int *subbuf, int *pos) {
    int temp;
    temp = hwread(vortex->mmio, VORTEX_WTDMA_STAT + (wtdma << 2));
    *subbuf = (temp >> WT_SUBBUF_SHIFT) & WT_SUBBUF_MASK;
    *pos = temp & POS_MASK;
}

int  vortex_wtdma_getcursubuffer(vortex_t *vortex, int wtdma) {
    return((hwread(vortex->mmio, VORTEX_ADBDMA_STAT + (wtdma << 2)) >> POS_SHIFT) & POS_MASK);
}

int inline vortex_wtdma_getlinearpos(vortex_t *vortex, int wtdma) {
    stream_t *dma = &vortex->dma_wt[wtdma];
    int temp;

    temp = hwread(vortex->mmio, VORTEX_WTDMA_STAT + (wtdma << 2));
    //temp = (temp & POS_MASK) + (((temp>>WT_SUBBUF_SHIFT) & WT_SUBBUF_MASK)*(dma->cfg0&POS_MASK));
	temp = (temp & POS_MASK) + ((dma->period_virt)*(dma->period_bytes));
    return temp;
}

void vortex_wtdma_startfifo(vortex_t *vortex, int wtdma) {
	stream_t *dma = &vortex->dma_wt[wtdma];
	int this_8=0, this_4=0;

	switch (dma->fifo_status) {
        case FIFO_START:
            vortex_fifo_setwtvalid(vortex, wtdma, dma->fifo_enabled ? 0xff:0);
            break;
        case FIFO_STOP:
			this_8 = 1;
            hwwrite(vortex->mmio, VORTEX_WTDMA_CTRL + (wtdma << 2), dma->dma_ctrl);
            vortex_fifo_setwtctrl(vortex, wtdma, dma->dma_unknown, this_4,
				this_8, dma->fifo_enabled ? 0xff:0, 0);
            break;
        case FIFO_PAUSE:
            vortex_fifo_setwtctrl(vortex, wtdma, dma->dma_unknown, this_4,
				this_8, dma->fifo_enabled ? 0xff:0, 0);
            break;
	}
	dma->fifo_status = FIFO_START;
}

void vortex_wtdma_resumefifo(vortex_t *vortex, int wtdma) {
	stream_t *dma = &vortex->dma_wt[wtdma];

    int this_8=0, this_4=0;
    switch (dma->fifo_status) {
        case FIFO_STOP:
            hwwrite(vortex->mmio, VORTEX_WTDMA_CTRL + (wtdma << 2), dma->dma_ctrl);
            vortex_fifo_setwtctrl(vortex, wtdma, dma->dma_unknown, this_4, this_8,
				dma->fifo_enabled ? 0xff:0, 0);
            break;
        case FIFO_PAUSE:
            vortex_fifo_setwtctrl(vortex, wtdma, dma->dma_unknown, this_4, this_8,
				dma->fifo_enabled ? 0xff:0, 0);
            break;
    }
    dma->fifo_status = FIFO_START;
}

void vortex_wtdma_pausefifo(vortex_t *vortex, int wtdma) {
	stream_t *dma = &vortex->dma_wt[wtdma];

    int this_8=0, this_4=0;
    switch (dma->fifo_status) {
        case FIFO_START:
            vortex_fifo_setwtctrl(vortex, wtdma, dma->dma_unknown, this_4, this_8, 0, 0);
            break;
        case FIFO_STOP:
            hwwrite(vortex->mmio, VORTEX_WTDMA_CTRL + (wtdma << 2), dma->dma_ctrl);
            vortex_fifo_setwtctrl(vortex, wtdma, dma->dma_unknown, this_4, this_8, 0, 0);
            break;
    }
    dma->fifo_status = FIFO_PAUSE;
}

void vortex_wtdma_stopfifo(vortex_t *vortex, int wtdma) {
	stream_t *dma = &vortex->dma_wt[wtdma];

    int this_4=0, this_8=0;
    if (dma->fifo_status == FIFO_START)
        vortex_fifo_setwtctrl(vortex, wtdma, dma->dma_unknown, this_4, this_8, 0, 0);
    else if (dma->fifo_status == FIFO_STOP)
        return;
    dma->fifo_status = FIFO_STOP;
    dma->fifo_enabled = 0;
}

void vortex_wtdma_chain(vortex_t *vortex, int wtdma, char sb1, char sb0) {
    stream_t *dma = &vortex->dma_wt[wtdma];
    int *cfg;
    int offset;

    if (sb0 == -1)
        sb1 = sb0;
    // this_0x80: cfg0, this_0x84: cfg1.

    switch (sb0) {
        case 0:
            dma->cfg0 = (dma->cfg0 & 0xcfffffff) | ((((sb1 & 0x3) | 4) <<4 )  << 24);
            cfg = &dma->cfg0;
            offset = VORTEX_WTDMA_BUFCFG0;
            break;
        case 1:
            dma->cfg0 = (dma->cfg0 & 0xfcffffff) | (((sb1 & 0x3) | 0x4) << 24);
            cfg = &dma->cfg0;
            offset = VORTEX_WTDMA_BUFCFG0;
            break;
        case 2:
            dma->cfg1 = (dma->cfg1 & 0xfcffffff) | ((((sb1 & 0x3) | 4) << 4) << 24);
            cfg = &dma->cfg1;
            offset = VORTEX_WTDMA_BUFCFG1;
            break;
        case 3:
            dma->cfg1 = (dma->cfg1 & 0xfcffffff) | (((sb1 & 0x3) | 0x4) << 24);
            cfg = &dma->cfg1;
            offset = VORTEX_WTDMA_BUFCFG1;
            break;
        default:
            return;
    }
    hwwrite(vortex->mmio, offset + (wtdma << 3), *cfg);
}

#endif
/* ADB */

typedef int ADBRamLink;
void vortex_adb_init(vortex_t *vortex) {
	int i;
	/* it looks like we are writing more than we need to...
	 * if we write what we are supposed to it breaks things... */
	hwwrite(vortex->mmio, VORTEX_ADB_SR, 0);
	for(i=0; i<VORTEX_ADB_RTBASE_SIZE; i++)
		hwwrite(vortex->mmio, VORTEX_ADB_RTBASE + (i<<2), 
				hwread(vortex->mmio, VORTEX_ADB_RTBASE + (i<<2)) | ROUTE_MASK);
	for(i=0; i<VORTEX_ADB_CHNBASE_SIZE; i++){
		hwwrite(vortex->mmio, VORTEX_ADB_CHNBASE + (i<<2), 
				hwread(vortex->mmio, VORTEX_ADB_CHNBASE + (i<<2)) | ROUTE_MASK);
	}
}

void vortex_adb_en_sr(vortex_t *vortex, int channel) {
    hwwrite(vortex->mmio, VORTEX_ADB_SR,
	    hwread(vortex->mmio, VORTEX_ADB_SR) | (0x1 << channel));
}

void vortex_adb_dis_sr(vortex_t *vortex, int channel) {
    hwwrite(vortex->mmio, VORTEX_ADB_SR,
	    hwread(vortex->mmio, VORTEX_ADB_SR) & ~(0x1 << channel));
}

void vortex_adb_addroutes(vortex_t *vortex, unsigned char channel, ADBRamLink *route, int rnum) {
    int temp, prev, lifeboat = 0;

    if ((rnum <= 0)||(route == NULL))
		return;
	/* Write last routes. */
	rnum--;
    hwwrite(vortex->mmio, VORTEX_ADB_RTBASE + ((route[rnum] & ADB_MASK) << 2), ROUTE_MASK);
    while (rnum > 0) {
	    hwwrite(vortex->mmio,	VORTEX_ADB_RTBASE + ((route[rnum-1] & ADB_MASK) << 2), route[rnum]);
	    rnum--;
    }
    /* Write first route. */
    temp = hwread(vortex->mmio, VORTEX_ADB_CHNBASE + (channel << 2)) & ADB_MASK;
    if (temp == ADB_MASK) {
	    /* First entry on this channel. */
	    hwwrite(vortex->mmio, VORTEX_ADB_CHNBASE + (channel << 2), route[0]);
	    vortex_adb_en_sr(vortex, channel);
	    return;
    }
    /* Not first entry on this channel. Need to link. */
    do {
		prev = temp;
	    temp = hwread(vortex->mmio, VORTEX_ADB_RTBASE + (temp << 2)) & ADB_MASK;
	    if ((lifeboat++) > ADB_MASK) {
	        printk(KERN_ERR "vortex_adb_addroutes: unending route!\n");
	        return;
	    }
    } while (temp != ADB_MASK);
    hwwrite(vortex->mmio, VORTEX_ADB_RTBASE + (prev << 2), route[0]);
}

void vortex_adb_delroutes(vortex_t *vortex, unsigned char channel, ADBRamLink route0, ADBRamLink route1) {
    int temp, lifeboat = 0, prev;

    /* Find route. */
    temp = hwread(vortex->mmio, VORTEX_ADB_CHNBASE + (channel << 2)) & ADB_MASK;
    if (temp == (route0 & ADB_MASK)) {
	    temp = hwread(vortex->mmio, VORTEX_ADB_RTBASE + ((route1 & ADB_MASK) << 2));
	    if ((temp & ADB_MASK) == ADB_MASK)
    	    vortex_adb_dis_sr(vortex, channel);
    	hwwrite(vortex->mmio, VORTEX_ADB_CHNBASE + (channel << 2), temp);
    	return;
    }
    do {
	    prev = temp;
	    temp = hwread(vortex->mmio, VORTEX_ADB_RTBASE + (prev << 2)) & ADB_MASK;
	    if (((lifeboat++) > ADB_MASK) || (temp == ADB_MASK)) {
    	    printk(KERN_ERR "vortex_adb_delroutes: route not found!\n");
	        return;
	    }
    } while (temp != (route0 & ADB_MASK));
    temp = hwread(vortex->mmio, VORTEX_ADB_RTBASE + (temp << 2));
    if ((temp & ADB_MASK) == route1)
        temp = hwread(vortex->mmio, VORTEX_ADB_RTBASE + (temp << 2));
    /* Make bridge over deleted route. */
    hwwrite(vortex->mmio, VORTEX_ADB_RTBASE + (prev << 2), temp);
}

void vortex_route(vortex_t *vortex, int en, unsigned char channel, unsigned char source, unsigned char dest) {
    ADBRamLink route;

    route = ((source & ADB_MASK) << ADB_SHIFT) | (dest & ADB_MASK);
    if (en) {
		vortex_adb_addroutes(vortex, channel, &route, 1);
		if ((source < (OFFSET_SRCOUT+NR_SRC)) && (source >= OFFSET_SRCOUT))
			vortex_src_addWTD(vortex, (source - OFFSET_SRCOUT), channel);
		else if ((source < (OFFSET_MIXOUT+NR_MIXOUT)) && (source >= OFFSET_MIXOUT))
			vortex_mixer_addWTD(vortex, (source - OFFSET_MIXOUT), channel);
    } else {
		vortex_adb_delroutes(vortex, channel, route, route);
		if ((source < (OFFSET_SRCOUT+NR_SRC)) && (source >= OFFSET_SRCOUT))
			vortex_src_delWTD(vortex, (source - OFFSET_SRCOUT), channel);
		else if ((source < (OFFSET_MIXOUT+NR_MIXOUT)) && (source >= OFFSET_MIXOUT))
			vortex_mixer_delWTD(vortex, (source - OFFSET_MIXOUT), channel);
    }
}

void vortex_routes(vortex_t *vortex, int en, unsigned char channel, unsigned char source,
    unsigned char dest0, unsigned char dest1) {
    ADBRamLink route[2];

    route[0] = ((source & ADB_MASK) << ADB_SHIFT) | (dest0 & ADB_MASK);
    route[1] = ((source & ADB_MASK) << ADB_SHIFT) | (dest1 & ADB_MASK);

    if (en) {
		vortex_adb_addroutes(vortex, channel, route, 2);
		if ((source < (OFFSET_SRCOUT+NR_SRC)) && (source >= (OFFSET_SRCOUT)))
			vortex_src_addWTD(vortex, (source - OFFSET_SRCOUT), channel);
		else if ((source < (OFFSET_MIXOUT+NR_MIXOUT)) && (source >= (OFFSET_MIXOUT)))
			vortex_mixer_addWTD(vortex, (source - OFFSET_MIXOUT), channel);
    } else {
		vortex_adb_delroutes(vortex, channel, route[0], route[1]);
		if ((source < (OFFSET_SRCOUT+NR_SRC)) && (source >= (OFFSET_SRCOUT)))
			vortex_src_delWTD(vortex, (source - OFFSET_SRCOUT), channel);
		else if ((source < (OFFSET_MIXOUT+NR_MIXOUT)) && (source >= (OFFSET_MIXOUT)))
			vortex_mixer_delWTD(vortex, (source - OFFSET_MIXOUT), channel);
    }
}

/* Route two sources to same target. Sources must be of same class !!! */
void vortex_routeLRT(vortex_t *vortex, int en, unsigned char ch, unsigned char source0,
    unsigned char source1, unsigned char dest) {
    ADBRamLink route[2];

    route[0] = ((source0 & ADB_MASK) << ADB_SHIFT) | (dest & ADB_MASK);
    route[1] = ((source1 & ADB_MASK) << ADB_SHIFT) | (dest & ADB_MASK);

	if (dest < 0x10)
		route[1] = (route[1] & ~ADB_MASK) | (dest + 0x20);

    if (en) {
		vortex_adb_addroutes(vortex, ch, route, 2);
		if ((source0 < (OFFSET_SRCOUT+NR_SRC)) && (source0 >= OFFSET_SRCOUT)) {
			vortex_src_addWTD(vortex, (source0 - OFFSET_SRCOUT), ch);
			vortex_src_addWTD(vortex, (source1 - OFFSET_SRCOUT), ch);
		}
		else if ((source0 < (OFFSET_MIXOUT+NR_MIXOUT)) && (source0 >= OFFSET_MIXOUT)) {
			vortex_mixer_addWTD(vortex, (source0 - OFFSET_MIXOUT), ch);
			vortex_mixer_addWTD(vortex, (source1 - OFFSET_MIXOUT), ch);
		}
    } else {
		vortex_adb_delroutes(vortex, ch, route[0], route[1]);
		if ((source0 < (OFFSET_SRCOUT+NR_SRC)) && (source0 >= OFFSET_SRCOUT)) {
			vortex_src_delWTD(vortex, (source0 - OFFSET_SRCOUT), ch);
			vortex_src_delWTD(vortex, (source1 - OFFSET_SRCOUT), ch);
		}
		else if ((source0 < (OFFSET_MIXOUT+NR_MIXOUT)) && (source0 >= OFFSET_MIXOUT)) {
			vortex_mixer_delWTD(vortex, (source0 - OFFSET_MIXOUT), ch);
			vortex_mixer_delWTD(vortex, (source1 - OFFSET_MIXOUT), ch);
		}
    }
}

/* Connection stuff */

// Connect adbdma to src('s).
void vortex_connection_adbdma_src(vortex_t *vortex, int en, unsigned char ch,
    unsigned char adbdma, unsigned char src) {
    vortex_route(vortex, en, ch, ADB_DMA(adbdma), ADB_SRCIN(src));
}

void vortex_connection_adbdma_src_src(vortex_t *vortex, int en, unsigned char channel,
    unsigned char adbdma, unsigned char src0, unsigned char src1) {
    vortex_routes(vortex, en, channel, ADB_DMA(adbdma), ADB_SRCIN(src0), ADB_SRCIN(src1));
}

// Connect SRC to mixin.
void vortex_connection_src_mixin(vortex_t *vortex, int en, unsigned char channel,
    unsigned char src, unsigned char mixin) {
    vortex_route(vortex, en, channel, ADB_SRCOUT(src), ADB_MIXIN(mixin));
}

// Connect mixin with mix output.
void vortex_connection_mixin_mix(vortex_t *vortex, int en, unsigned char mixin, unsigned char mix, int a) {
    if (en) {
		vortex_mix_enableinput(vortex, mix, mixin);
        vortex_mix_setinputvolumebyte(vortex, mix, mixin, MIX_DEFIGAIN);	// added to original code.
    } else
	    vortex_mix_disableinput(vortex, mix, mixin, a);
}

// Connect absolut address to mixin.
void vortex_connection_adb_mixin(vortex_t *vortex, int en, unsigned char channel,
    unsigned char source, unsigned char mixin) {
    vortex_route(vortex, en, channel, source, ADB_MIXIN(mixin));
}

// Connect two mix to AdbDma.
void vortex_connection_mix_mix_adbdma(vortex_t *vortex, int en, unsigned char ch,
    unsigned char mix0, unsigned char mix1, unsigned char adbdma) {

    ADBRamLink routes[2];
    routes[0] = (((mix0 + OFFSET_MIXOUT) & ADB_MASK) << ADB_SHIFT) | (adbdma & ADB_MASK);
    routes[1] = (((mix1 + OFFSET_MIXOUT) & ADB_MASK) << ADB_SHIFT) | ((adbdma + 0x20 /*OFFSET_MIXOUT*/) & ADB_MASK);
    if (en) {
	    vortex_adb_addroutes(vortex, ch, routes, 0x2);
		vortex_mixer_addWTD(vortex, mix0, ch);
		vortex_mixer_addWTD(vortex, mix1, ch);
    }
	else {
		vortex_adb_delroutes(vortex, ch, routes[0], routes[1]);
		vortex_mixer_delWTD(vortex, mix0, ch);
		vortex_mixer_delWTD(vortex, mix1, ch);
    }
}

void vortex_connection_src_adbdma(vortex_t *vortex, int en, unsigned char ch,
    unsigned char src, unsigned char adbdma) {
    vortex_route(vortex, en, ch, ADB_SRCOUT(src), ADB_DMA(adbdma));
}

void vortex_connection_src_src_adbdma(vortex_t *vortex, int en, unsigned char ch,
    unsigned char src0, unsigned char src1, unsigned char adbdma) {

	vortex_routeLRT(vortex, en, ch, ADB_SRCOUT(src0), ADB_SRCOUT(src1), ADB_DMA(adbdma));
}

// mix to absolut address.
void vortex_connection_mix_adb(vortex_t *vortex, int en, unsigned char ch,
    unsigned char mix, unsigned char dest) {
    vortex_route(vortex, en, ch, ADB_MIXOUT(mix), dest);
    vortex_mix_setvolumebyte(vortex, mix, MIX_DEFOGAIN);	// added to original code.
}

// mixer to src.
void vortex_connection_mix_src(vortex_t *vortex, int en, unsigned char ch,
    unsigned char mix, unsigned char src) {
    vortex_route(vortex, en, ch, ADB_MIXOUT(mix), ADB_SRCIN(src));
    vortex_mix_setvolumebyte(vortex, mix, MIX_DEFOGAIN);	// added to original code.
}

/* CODEC connect. */
void vortex_connect_codecplay(vortex_t *vortex, int en, unsigned char mixers[]) {
#ifdef CHIP_AU8820
	vortex_connection_mix_adb(vortex, en, 0x11, mixers[0], ADB_CODECOUT(0));
	vortex_connection_mix_adb(vortex, en, 0x11, mixers[1], ADB_CODECOUT(1));
#else
#if 1
	// Connect front channels through EQ.
	vortex_connection_mix_adb(vortex, en, 0x11, mixers[0], ADB_EQIN(0));
	vortex_connection_mix_adb(vortex, en, 0x11, mixers[1], ADB_EQIN(1));
    vortex_route(vortex, en, 0x11, ADB_EQOUT(0), ADB_CODECOUT(0));
    vortex_route(vortex, en, 0x11, ADB_EQOUT(1), ADB_CODECOUT(1));
	/* Check if reg 0x28 has SDAC bit set. */
	if (VORTEX_IS_QUAD(vortex)) {
		/* Rear channel. Note: ADB_CODECOUT(0+2) and (1+2) is for AC97 modem */
		vortex_connection_mix_adb(vortex, en, 0x11, mixers[2], ADB_CODECOUT(0+4));
		vortex_connection_mix_adb(vortex, en, 0x11, mixers[3], ADB_CODECOUT(1+4));
		printk("SDAC detected ");
	}
#else
	// Use plain direct output to codec.
	vortex_connection_mix_adb(vortex, en, 0x11, mixers[0], ADB_CODECOUT(0));
	vortex_connection_mix_adb(vortex, en, 0x11, mixers[1], ADB_CODECOUT(1));
#endif	
#endif
}

void vortex_connect_codecrec(vortex_t *vortex, int en, unsigned char mixin0, unsigned char mixin1) {
	/*
	 Enable: 0x1, 0x1
	 Channel: 0x11, 0x11
	 ADB Source address: 0x48, 0x49
	 Destination Asp4Topology_0x9c,0x98
	*/
	vortex_connection_adb_mixin(vortex, en, 0x11, ADB_CODECIN(0), mixin0);
	vortex_connection_adb_mixin(vortex, en, 0x11, ADB_CODECIN(1), mixin1);
}

// Higher level audio path (de)allocator.

/* Resource manager */
static int resnum[VORTEX_RESOURCE_LAST] = {NR_ADB, NR_SRC, NR_MIXIN, NR_MIXOUT, NR_A3D};
/*
 Checkout/Checkin resource of given type. 
 stream: resource map to be used. If NULL means that we want to allocate
 the DMA resource (root of all other resources).
 out: Mean checkout if != 0. Else mean Checkin resource.
 restype: Indicates type of resource to be checked in or out.
*/
int  vortex_adb_checkinout(vortex_t *vortex, int resmap[], int out, int restype) {
	int i, qty = resnum[restype], resinuse=0;
         
	if (out) {
		/* Gather used resources by all streams. */
		for (i=0; i<NR_ADB; i++) {
			resinuse |= vortex->dma_adb[i].resources[restype];
		}
		resinuse |= vortex->fixed_res[restype];
		/* Find and take free resource. */
		for (i=0; i<qty; i++) {
			if ((resinuse & (1 << i)) == 0) {
				if (resmap != NULL)
					resmap[restype] |= (1 << i);
				else
					vortex->dma_adb[i].resources[restype] |= (1 << i);
				//printk("vortex: ResManager: type %d out %d\n", restype, i);
				return i;
			}
		}
	} else {
		if (resmap == NULL)
			return -EINVAL;
		/* Checkin first resource of type restype. */
		for (i=0; i<qty; i++) {
			if (resmap[restype] & (1 << i)) {
				resmap[restype] &= ~(1 << i);
				//printk("vortex: ResManager: type %d in %d\n",restype, i);
				return i;
			}
		}
		if (vortex->fixed_res[restype] & (1 << i)) {
				vortex->fixed_res[restype] &= ~(1 << i);
				//printk("vortex: ResManager (fixed rsc): type %d in %d\n",restype, i);
				return i;
		}
	}
	printk("vortex: ResManager: type %d FATAL\n", restype);
	return -ENOMEM;
}

/* Default Connections  */
void vortex_connect_default(vortex_t *vortex, int en) {
    
	// FIXME: check if checkout was succesful.
	// Connect AC97 codec.
	vortex->mixplayb[0] = vortex_adb_checkinout(vortex, vortex->fixed_res, en, VORTEX_RESOURCE_MIXOUT);
	vortex->mixplayb[1] = vortex_adb_checkinout(vortex, vortex->fixed_res, en, VORTEX_RESOURCE_MIXOUT);
	if (VORTEX_IS_QUAD(vortex)) {
		vortex->mixplayb[2] = vortex_adb_checkinout(vortex, vortex->fixed_res, en, VORTEX_RESOURCE_MIXOUT);
		vortex->mixplayb[3] = vortex_adb_checkinout(vortex, vortex->fixed_res, en, VORTEX_RESOURCE_MIXOUT);
	}
	vortex_connect_codecplay(vortex, en, vortex->mixplayb);
	
	vortex->mixcapt[0] = vortex_adb_checkinout(vortex, vortex->fixed_res, en, VORTEX_RESOURCE_MIXIN);
	vortex->mixcapt[1] = vortex_adb_checkinout(vortex, vortex->fixed_res, en, VORTEX_RESOURCE_MIXIN);
	vortex_connect_codecrec(vortex, en, MIX_CAPT(0), MIX_CAPT(1));
	
	// Connect SPDIF
	vortex->mixspdif[0] = vortex_adb_checkinout(vortex, vortex->fixed_res, en, VORTEX_RESOURCE_MIXOUT);
	vortex->mixspdif[1] = vortex_adb_checkinout(vortex, vortex->fixed_res, en, VORTEX_RESOURCE_MIXOUT);
	vortex_connection_mix_adb(vortex, en, 0x14, vortex->mixspdif[0], ADB_SPDIFOUT(0));
	vortex_connection_mix_adb(vortex, en, 0x14, vortex->mixspdif[1], ADB_SPDIFOUT(1));
	
	// Connect I2S
	
	// Connect DSP interface (not here i think...)
	
	// Connect MODEM
	
#ifndef CHIP_AU8810
	//vortex_wt_connect(vortex, en, vortex->mixplayb);
#endif	
	/* Fast Play Workaround */
#ifndef CHIP_AU8820
	vortex->fixed_res[VORTEX_RESOURCE_DMA] = 0x00000001;
#endif
}

/*
  Allocate nr_ch pcm audio routes if dma < 0. If dma >= 0, existing routes
  are deallocated.
  dma: DMA engine routes to be deallocated when dma >= 0.
  nr_ch: Number of channels to be de/allocated.
  dir: direction of stream. Uses same values as substream->stream.
  type: Type of audio output/source (codec, spdif, i2s, dsp, etc)
  Return: Return allocated DMA or same DMA passed as "dma" when dma >= 0.
*/
int  vortex_adb_allocroute(vortex_t *vortex, int dma, int nr_ch, int dir, int type) {
	stream_t *stream;
	int i, en;
	
	if ((nr_ch == 3) || ((dir == SNDRV_PCM_STREAM_CAPTURE)&&(nr_ch > 2)))
		return -EBUSY;
	
	spin_lock(vortex->lock);
	if (dma >= 0) {
   		en = 0;
		vortex_adb_checkinout(vortex, vortex->dma_adb[dma].resources, en, VORTEX_RESOURCE_DMA);
	} else {
		en = 1;
		if ((dma = vortex_adb_checkinout(vortex, NULL, en, VORTEX_RESOURCE_DMA)) < 0)
			return -EBUSY;
	}
	
	stream = &vortex->dma_adb[dma];
	stream->dma = dma;
	stream->dir = dir;
	stream->type = type;	
	
	// FIXME: check for success of checkout or checkin.
	if (dir == SNDRV_PCM_STREAM_PLAYBACK) {
		int src[4], mix[4], a3d=0, ch_top;
		
		/* Get SRC and MIXER hardware resources. */
		if (stream->type != VORTEX_PCM_SPDIF) {
			for (i=0; i<nr_ch; i++) {
				if ((src[i] = vortex_adb_checkinout(vortex, stream->resources, en, VORTEX_RESOURCE_SRC))<0) {
					memset(stream->resources, 0, sizeof(unsigned char)*VORTEX_RESOURCE_LAST);
					return -EBUSY;
				}
				if ((mix[i] = vortex_adb_checkinout(vortex, stream->resources, en, VORTEX_RESOURCE_MIXIN))<0) {
					memset(stream->resources, 0, sizeof(unsigned char)*VORTEX_RESOURCE_LAST);
					return -EBUSY;
				}
			}
		}
		if (stream->type == VORTEX_PCM_A3D) {
			if ((a3d = vortex_adb_checkinout(vortex, stream->resources, en, VORTEX_RESOURCE_A3D))<0) {
				memset(stream->resources, 0, sizeof(unsigned char)*VORTEX_RESOURCE_LAST);
				return -EBUSY;
			}
		}
		/* Make SPDIF out exclusive to "spdif" device when in use. */
		if ((stream->type == VORTEX_PCM_SPDIF)&&(en)) {
			vortex_route(vortex, 0, 0x14, ADB_MIXOUT(vortex->mixspdif[0]), ADB_SPDIFOUT(0));
			vortex_route(vortex, 0, 0x14, ADB_MIXOUT(vortex->mixspdif[1]), ADB_SPDIFOUT(1));
		}
		/* Make playback routes. */
		for (i=0; i<nr_ch; i++) {
			if (stream->type == VORTEX_PCM_ADB) {
				vortex_connection_adbdma_src(vortex, en, src[nr_ch-1], dma, src[i]);
				vortex_connection_src_mixin(vortex, en, 0x11, src[i], mix[i]);
				vortex_connection_mixin_mix(vortex, en, mix[i], MIX_PLAYB(i), 0);
				vortex_connection_mixin_mix(vortex, en, mix[i], MIX_SPDIF(i % 2), 0);
				vortex_mix_setinputvolumebyte(vortex, MIX_SPDIF(i % 2), mix[i], MIX_DEFIGAIN);
			}
			if (stream->type == VORTEX_PCM_A3D) {
				vortex_connection_adbdma_src(vortex, en, src[nr_ch-1], dma, src[i]);
				vortex_route(vortex, en, 0x11, ADB_SRCOUT(src[i]), ADB_A3DIN(a3d));
				vortex_route(vortex, en, 0x11, ADB_A3DOUT(a3d), ADB_MIXIN(mix[i]));
				vortex_connection_mixin_mix(vortex, en, mix[i], MIX_PLAYB(i), 0);
				vortex_connection_mixin_mix(vortex, en, mix[i], MIX_SPDIF(i % 2), 0);				
				vortex_mix_setinputvolumebyte(vortex, MIX_SPDIF(i % 2), mix[i], MIX_DEFIGAIN);
			}
			if (stream->type == VORTEX_PCM_SPDIF)
				vortex_route(vortex, en, 0x14, ADB_DMA(stream->dma), ADB_SPDIFOUT(i));
		}
		if (stream->type != VORTEX_PCM_SPDIF) {
			ch_top = (VORTEX_IS_QUAD(vortex) ? 4 : 2);
				for (i=nr_ch; i<ch_top; i++) {
					vortex_connection_mixin_mix(vortex, en, mix[i % nr_ch], MIX_PLAYB(i), 0);
					vortex_connection_mixin_mix(vortex, en, mix[i % nr_ch], MIX_SPDIF(i % 2), 0);
					vortex_mix_setinputvolumebyte(vortex, MIX_SPDIF(i % 2), mix[i % nr_ch], MIX_DEFIGAIN);
				}
		} else {
			if (nr_ch == 1)
				vortex_route(vortex, en, 0x14, ADB_DMA(stream->dma), ADB_SPDIFOUT(1));
		}
		/* Reconnect SPDIF out when "spdif" device is down. */
		if ((stream->type == VORTEX_PCM_SPDIF)&&(!en)) {
			vortex_route(vortex, 1, 0x14, ADB_MIXOUT(vortex->mixspdif[0]), ADB_SPDIFOUT(0));
			vortex_route(vortex, 1, 0x14, ADB_MIXOUT(vortex->mixspdif[1]), ADB_SPDIFOUT(1));
		}		
	} else {
		int src[2], mix[2];
		
		/* Get SRC and MIXER hardware resources. */
		for (i=0; i<nr_ch; i++) {
			if ((mix[i] = vortex_adb_checkinout(vortex, stream->resources, en, VORTEX_RESOURCE_MIXOUT))<0) {
				memset(stream->resources, 0, sizeof(unsigned char)*VORTEX_RESOURCE_LAST);
				return -EBUSY;
			}
			if ((src[i] = vortex_adb_checkinout(vortex, stream->resources, en, VORTEX_RESOURCE_SRC))<0) {
				memset(stream->resources, 0, sizeof(unsigned char)*VORTEX_RESOURCE_LAST);
				return -EBUSY;
			}
		}
		
		/* Make capture routes. */
		vortex_connection_mixin_mix(vortex, en, MIX_CAPT(0), mix[0], 0);
		vortex_connection_mix_src(vortex, en, 0x11, mix[0], src[0]);
		if (nr_ch == 1) {
			vortex_connection_mixin_mix(vortex, en, MIX_CAPT(1), mix[0], 0);
			vortex_connection_src_adbdma(vortex, en, src[nr_ch-1], src[0], dma);
		} else {
			vortex_connection_mixin_mix(vortex, en, MIX_CAPT(1), mix[1], 0);
			vortex_connection_mix_src(vortex, en, 0x11, mix[1], src[1]);
			vortex_connection_src_src_adbdma(vortex, en, src[0], src[0], src[1], dma);
		}
	}
	vortex->dma_adb[dma].nr_ch = nr_ch;
	spin_unlock(vortex->lock);
	
#if 0
	/* AC97 Codec channel setup. FIXME: this has no effect !! */
	if (nr_ch < 4) {
		/* Copy stereo to rear channel (surround) */
		snd_ac97_write_cache(vortex->codec, AC97_SIGMATEL_DAC2INVERT, snd_ac97_read(vortex->codec, AC97_SIGMATEL_DAC2INVERT) | 4);
	} else {
		/* Allow separate front a rear channels. */
		snd_ac97_write_cache(vortex->codec, AC97_SIGMATEL_DAC2INVERT, snd_ac97_read(vortex->codec, AC97_SIGMATEL_DAC2INVERT) & ~((u32)4));
	}
#endif
	return dma;
}
/*
 Set the SampleRate of the SRC's attached to the given DMA engine.
 */
void vortex_adb_setsrc(vortex_t *vortex, int adbdma, unsigned int rate, int dir) {
    stream_t *stream = &(vortex->dma_adb[adbdma]);
    int i, cvrt;
    	
	/* dir=1:play ; dir=0:rec */
    if (dir)
        cvrt = SRC_RATIO(rate,48000);
    else
        cvrt = SRC_RATIO(48000,rate);
	
	/* Setup SRC's */
	for (i=0; i<NR_SRC; i++) {
		if (stream->resources[VORTEX_RESOURCE_SRC] & (1<<i))
			vortex_src_setupchannel(vortex, i, cvrt, 0, 0, i, dir, 0, cvrt, dir);
	}
}

// Timer and ISR functions.

void vortex_settimer(vortex_t *vortex, int period) {
    //set the timer period to <period> 48000ths of a second.
	hwwrite(vortex->mmio, VORTEX_IRQ_STAT, period);
}

void vortex_enable_timer_int(vortex_t *card){
	hwwrite(card->mmio, VORTEX_IRQ_CTRL, hwread(card->mmio, VORTEX_IRQ_CTRL) | IRQ_TIMER | 0x60);
}

void vortex_disable_timer_int(vortex_t *card){
	hwwrite(card->mmio, VORTEX_IRQ_CTRL, hwread(card->mmio, VORTEX_IRQ_CTRL) & ~IRQ_TIMER);
}

void vortex_enable_int(vortex_t *card){
	// CAsp4ISR__EnableVortexInt_void_
	hwwrite(card->mmio, VORTEX_CTRL, hwread(card->mmio, VORTEX_CTRL) | CTRL_IRQ_ENABLE);
	hwwrite(card->mmio, VORTEX_IRQ_CTRL, (hwread(card->mmio, VORTEX_IRQ_CTRL) & 0xffffefc0) | 0x24);
}

void vortex_disable_int(vortex_t *card){
	hwwrite(card->mmio, VORTEX_CTRL, hwread(card->mmio, VORTEX_CTRL) & ~CTRL_IRQ_ENABLE);
}

irqreturn_t vortex_interrupt(int irq, void *dev_id, struct pt_regs *regs) {
	vortex_t *vortex = snd_magic_cast(vortex_t, dev_id, return IRQ_NONE);
	int i, handled;
	u32 source;	
	//check if the interrupt is ours.
	if (!(hwread(vortex->mmio, VORTEX_IRQ_U0) & 0x1))
		return IRQ_NONE;
	
	// This is the Interrrupt Enable flag we set before (consistency check).
	if ((hwread(vortex->mmio, VORTEX_CTRL) & 0x0000ff00) == CTRL_IRQ_ENABLE)
		return IRQ_HANDLED;
	
	source = hwread(vortex->mmio, VORTEX_IRQ_SOURCE);
	// Reset IRQ flags.
	hwwrite(vortex->mmio, VORTEX_IRQ_SOURCE, source);
	hwread(vortex->mmio, VORTEX_IRQ_SOURCE);
	// Is at least one IRQ flag set?
	if (source == 0) {
		printk(KERN_ERR "vortex: missing irq source\n");
		return IRQ_NONE;
	}
	
	//printk(KERN_INFO "IRQ: 0x%x\n", source);

	handled = 0;
	// Attend every interrupt source.
	if (source & IRQ_FATAL) {
		printk(KERN_ERR "vortex: IRQ fatal error\n");
		handled = 1;
	}
	if (source & IRQ_PARITY) {
		printk(KERN_ERR "vortex: IRQ parity error\n");
		handled = 1;
	}
	if (source & IRQ_PCMOUT) {
		/* ALSA period acknowledge. */
		for (i=0; i<NR_ADB; i++) {
			if (vortex->dma_adb[i].fifo_status == FIFO_START) {
				vortex_adbdma_bufshift(vortex, i);
				snd_pcm_period_elapsed(vortex->dma_adb[i].substream);
			}
		}
#ifndef CHIP_AU8810
		for (i=0; i<NR_WT; i++) {
			if (vortex->dma_wt[i].fifo_status == FIFO_START) {
				vortex_wtdma_bufshift(vortex, i);
				snd_pcm_period_elapsed(vortex->dma_wt[i].substream);
			}
		}
#endif
		handled = 1;
	}
	//Acknowledge the Timer interrupt
	if (source & IRQ_TIMER) {
		hwread(vortex->mmio, VORTEX_IRQ_STAT);
		handled = 1;
	}
	if (source & IRQ_MIDI) {
		snd_mpu401_uart_interrupt(vortex->irq, vortex->rmidi->private_data, regs);
		handled = 1;
	}
	
	if (!handled) {
		printk(KERN_ERR "vortex: unknown irq source %x\n", source);
	}
	return IRQ_RETVAL(handled);
}

/* Codec */

#define POLL_COUNT 1000
void vortex_codec_init(vortex_t *vortex) {
    int i;

	for(i =0; i<32; i++){
		hwwrite(vortex->mmio,(VORTEX_CODEC_CHN+(i<<2)),0);
		udelay(2000);
	}
	if (0) {
		hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0x8068);
		udelay(1000);
		hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0x00e8);
		udelay(1000);
	} else {
		hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0x00a8);
		udelay(2000);
		hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0x80a8);
		udelay(2000);
		hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0x80e8);
		udelay(2000);
		hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0x80a8);
		udelay(2000);
		hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0x00a8);
		udelay(2000);
		hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0x00e8);
	}
	for(i =0; i<32; i++){
		hwwrite(vortex->mmio,(VORTEX_CODEC_CHN+(i<<2)),0);
		udelay(5000);
	}
	hwwrite(vortex->mmio,VORTEX_CODEC_CTRL,0xe8);
	udelay(1000);
	/* Enable codec channels 0 and 1. */
	hwwrite(vortex->mmio,VORTEX_CODEC_EN, hwread(vortex->mmio,VORTEX_CODEC_EN) | EN_CODEC);
}

void vortex_codec_write(ac97_t *codec, unsigned short addr, unsigned short data){

	vortex_t *card = (vortex_t*)codec->private_data;
	unsigned long flags;
	unsigned int lifeboat = 0;
	spin_lock_irqsave(&card->lock, flags);
	
	/* wait for transactions to clear */
	while (!(hwread(card->mmio, VORTEX_CODEC_CTRL) & 0x100)) {
		udelay(100);
		if (lifeboat++ > POLL_COUNT) {
			printk(KERN_ERR "vortex: ac97 codec stuck busy\n");
			spin_unlock_irqrestore(&card->lock, flags);
			return;
		}
	}
	/* write register */
 	hwwrite(card->mmio, VORTEX_CODEC_IO, ((addr << VORTEX_CODEC_ADDSHIFT) & VORTEX_CODEC_ADDMASK) |
	 		((data << VORTEX_CODEC_DATSHIFT) & VORTEX_CODEC_DATMASK) | VORTEX_CODEC_WRITE);
	
	/* Flush Caches.*/
	hwread(card->mmio, VORTEX_CODEC_IO);
	
	spin_unlock_irqrestore(&card->lock, flags);
}

unsigned short vortex_codec_read(ac97_t *codec, unsigned short addr) {

	vortex_t *card = (vortex_t*)codec->private_data;
	u32 read_addr, data;
	unsigned long flags;
	unsigned lifeboat = 0;
	
	spin_lock_irqsave(&card->lock, flags);

	/* wait for transactions to clear */
	while (!(hwread(card->mmio, VORTEX_CODEC_CTRL) & 0x100)) {
		udelay(100);
		if (lifeboat++ > POLL_COUNT) {
			printk(KERN_ERR "vortex: ac97 codec stuck busy\n");
			spin_unlock_irqrestore(&card->lock, flags);
			return 0xffff;
		}
	}	
	/* set up read address */
	read_addr = ((addr << VORTEX_CODEC_ADDSHIFT) & VORTEX_CODEC_ADDMASK);
	hwwrite(card->mmio, VORTEX_CODEC_IO, read_addr);

	/* wait for address */
	{
		udelay(100);
		data = hwread(card->mmio, VORTEX_CODEC_IO);
		if (lifeboat++ > POLL_COUNT) {
			printk(KERN_ERR "vortex: ac97 address never arrived\n");
			spin_unlock_irqrestore(&card->lock, flags);
			return 0xffff;
		}
	} while ((data & VORTEX_CODEC_ADDMASK) != (addr << VORTEX_CODEC_ADDSHIFT));
	
	/* Unlock. */
	spin_unlock_irqrestore(&card->lock, flags);
	
	/* return data. */
	return (u16)(data & VORTEX_CODEC_DATMASK);
}

/* SPDIF support  */
void vortex_spdif_init(vortex_t *vortex, int spdif_sr, int spdif_mode) {
	int i, this_38 = 0, this_04=0, this_08=0, this_0c=0;
	
	/* CAsp4Spdif::InitializeSpdifHardware(void) */
	hwwrite(vortex->mmio, VORTEX_SPDIF_FLAGS, hwread(vortex->mmio, VORTEX_SPDIF_FLAGS) & 0xfff3fffd);
	//for (i=0x291D4; i<0x29200; i+=4)
	for (i=0; i<11; i++)
		hwwrite(vortex->mmio, VORTEX_SPDIF_CFG1 + (i<<2), 0);
	//hwwrite(vortex->mmio, 0x29190, hwread(vortex->mmio, 0x29190) | 0xc0000);
	hwwrite(vortex->mmio, VORTEX_CODEC_EN, hwread(vortex->mmio, VORTEX_CODEC_EN) | EN_SPDIF);

	/* CAsp4Spdif::ProgramSRCInHardware(enum  SPDIF_SR,enum	 SPDIFMODE) */
	if (this_04 && this_08) {
		int edi;
		
		i = (((0x5DC00000 / spdif_sr) + 1) >> 1);
		if (i > 0x800) {
			if (i < 0x1ffff)
				edi = (i >> 1);
			else 
				edi = 0x1ffff;
		} else {
			i = edi = 0x800;
		}
		/* this_04 and this_08 are the CASp4Src's (samplerate converters) */
		vortex_src_setupchannel(vortex, this_04, edi, 0, 1, this_0c, 1, 0, edi, 1);
		vortex_src_setupchannel(vortex, this_08, edi, 0, 1, this_0c, 1, 0, edi, 1);
	}
	
	i = spdif_sr;
	spdif_sr |= 0x8c;
	switch (i) {
		case 32000:
			this_38 &= 0xFFFFFFFE;
			this_38 &= 0xFFFFFFFD;
			this_38 &= 0xF3FFFFFF;
			this_38 |= 0x03000000;
			this_38 &= 0xFFFFFF3F;
			spdif_sr &= 0xFFFFFFFD;
			spdif_sr |= 1;
			break;
		case 44100:
			this_38 &= 0xFFFFFFFE;
			this_38 &= 0xFFFFFFFD;
			this_38 &= 0xF0FFFFFF;
			this_38 |= 0x03000000;
			this_38 &= 0xFFFFFF3F;
			spdif_sr &= 0xFFFFFFFC;
			break;
		case 48000:
			if (spdif_mode == 1) {
				this_38 &= 0xFFFFFFFE;
				this_38 &= 0xFFFFFFFD;
				this_38 &= 0xF2FFFFFF;
				this_38 |= 0x02000000;
				this_38 &= 0xFFFFFF3F;
			} else {
				this_38 |= 0x00000003;
				this_38 &= 0xFFFFFFBF;
				this_38 |= 0x80;
			}
			spdif_sr |= 2;
			spdif_sr &= 0xFFFFFFFE;
			break;
		
	}
	hwwrite(vortex->mmio, VORTEX_SPDIF_CFG0, this_38 & 0xffff);
	hwwrite(vortex->mmio, VORTEX_SPDIF_CFG1, this_38 >> 0x10);
	hwwrite(vortex->mmio, VORTEX_SPDIF_SMPRATE, spdif_sr);
}

/* Initialization */

int vortex_core_init(vortex_t *vortex) {

	printk(KERN_INFO "Vortex: hardware init.... ");
	/* Hardware Init. */
	hwwrite(vortex->mmio, VORTEX_CTRL, 0xffffffff);
    udelay(5000);
    hwwrite(vortex->mmio, VORTEX_CTRL, hwread(vortex->mmio, VORTEX_CTRL) & 0xffdfffff);
	udelay(5000);
	/* Reset IRQ flags */
	hwwrite(vortex->mmio, VORTEX_IRQ_SOURCE, 0xffffffff);
	hwread(vortex->mmio, VORTEX_IRQ_STAT);

	vortex_codec_init(vortex);
	
#ifdef CHIP_AU8830
	hwwrite(vortex->mmio, VORTEX_CTRL, hwread(vortex->mmio, VORTEX_CTRL) | 0x1000000);
#endif	
	
    /* Audio engine init. */
	vortex_fifo_init(vortex);
	hwwrite(vortex->mmio, VORTEX_ENGINE_CTRL, 0x0);	//, 0xc83c7e58, 0xc5f93e58
	vortex_adb_init(vortex);
	/* Routing blocks init. */
	vortex_adbdma_init(vortex);
	vortex_mixer_init(vortex);
	vortex_srcblock_init(vortex);
#ifndef CHIP_AU8820
	vortex_eq_init(vortex);
	vortex_spdif_init(vortex, 48000, 1);
#endif
#ifndef CHIP_AU8810
	vortex_wt_InitializeWTRegs(vortex);
#endif
	// Moved to au88x0.c
	//vortex_connect_default(vortex, 1);

    vortex_settimer(vortex, 0x90);
    // Enable Interrupts.
    // vortex_enable_int() must be first !!
    //	hwwrite(vortex->mmio, VORTEX_IRQ_CTRL, 0);
    // vortex_enable_int(vortex);
    //vortex_enable_timer_int(vortex);
    //vortex_disable_timer_int(vortex);

    printk(KERN_INFO "done.\n");
    spin_lock_init(&vortex->lock);

    return 0;
}

int vortex_core_shutdown(vortex_t *vortex) {

    printk(KERN_INFO "Vortex: hardware shutdown...");
#ifndef CHIP_AU8820	
	vortex_eq_free(vortex);
#endif
    vortex_disable_timer_int(vortex);
	vortex_disable_int(vortex);
    vortex_connect_default(vortex, 0);
    /* Reset all DMA fifos. */
	vortex_fifo_init(vortex);
    /* Erase all audio routes. */
	vortex_adb_init(vortex);

    /* Disable MPU401 */
	//hwwrite(vortex->mmio, VORTEX_IRQ_CTRL, hwread(vortex->mmio, VORTEX_IRQ_CTRL) & ~IRQ_MIDI);
	//hwwrite(vortex->mmio, VORTEX_CTRL, hwread(vortex->mmio, VORTEX_CTRL) & ~CTRL_MIDI_EN);

    hwwrite(vortex->mmio, VORTEX_IRQ_CTRL, 0);
	hwwrite(vortex->mmio, VORTEX_CTRL, 0);
	udelay(5000);
	hwwrite(vortex->mmio, VORTEX_IRQ_SOURCE, 0xffff);

    printk(KERN_INFO "done.\n");
    return 0;
}

/* Alsa support. */

int vortex_alsafmt_aspfmt(int alsafmt) {
    int fmt;

    switch (alsafmt) {
        case SNDRV_PCM_FORMAT_U8: fmt = 0x1;
            break;
        case SNDRV_PCM_FORMAT_MU_LAW: fmt = 0x2;
            break;
        case SNDRV_PCM_FORMAT_A_LAW: fmt = 0x3;
            break;
        case SNDRV_PCM_FORMAT_SPECIAL: fmt = 0x4; /* guess. */
            break;
        case SNDRV_PCM_FORMAT_IEC958_SUBFRAME_LE: fmt = 0x5; /* guess. */
            break;
        case SNDRV_PCM_FORMAT_S16_LE: fmt = 0x8;
            break;
        default: fmt = 0x8;
            printk(KERN_ERR "vortex: format unsupported %d\n", alsafmt);
            break;
    }
    return fmt;
}

/* Some not yet useful translations. */

typedef enum {
	ASPFMTLINEAR16 = 0, /* 0x8 */
	ASPFMTLINEAR8,      /* 0x1 */
	ASPFMTULAW,         /* 0x2 */
	ASPFMTALAW,         /* 0x3 */
	ASPFMTSPORT,        /* ? */
	ASPFMTSPDIF,        /* ? */
} ASPENCODING;

int  vortex_translateformat(vortex_t *vortex, char bits, char nch, int encod) {
	int a, this_194;

	if ((bits != 8) || (bits != 16))
		return -1;

	switch (encod) {
		case 0:
			if (bits == 0x10)
				a = 8; // 16 bit
			break;
		case 1:
			if (bits == 8)
				a = 1; // 8 bit
			break;
		case 2: a = 2; // U_LAW
			break;
		case 3: a = 3; // A_LAW
			break;
	}
	switch (nch) {
		case 1: this_194 = 0;
			break;
		case 2:	this_194 = 1;
			break;
		case 4: this_194 = 1;
			break;
		case 6: this_194 = 1;
			break;
	}
	return(a);
}

void vortex_cdmacore_setformat(vortex_t *vortex, int bits, int nch) {
	short int d, this_148;

	d = ((bits >> 3)*nch);
	this_148 = 0xbb80 / d;
}
