#ifndef __EMU8000_H
#define __EMU8000_H
/*
 *  Defines for the emu8000 (AWE32/64)
 *
 *  Copyright (C) 1999 Steve Ratcliffe
 *  Copyright (C) 1999-2000 Takashi Iwai <tiwai@suse.de>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "driver.h"
#include "emux_synth.h"
#include "seq_kernel.h"

/*
 * Hardware parameters.
 */
#define EMU8000_MAX_DRAM (28 * 1024 * 1024) /* Max on-board mem is 28Mb ???*/
#define EMU8000_DRAM_OFFSET 0x200000	/* Beginning of on board ram */
#define EMU8000_CHANNELS   32	/* Number of hardware channels */
#define EMU8000_DRAM_VOICES	30	/* number of normal voices */

/* Flags to set a dma channel to read or write */
#define EMU8000_RAM_READ   0
#define EMU8000_RAM_WRITE  1
#define EMU8000_RAM_CLOSE  2

/*
 * Structure to hold all state information for the emu8000 driver.
 *
 * Note 1: The chip supports 32 channels in hardware this is max_channels
 * some of the channels may be used for other things so max_channels is
 * the number in use for wave voices.
 */
typedef struct snd_emu8000 {

	snd_emux_t *emu;

	int mem_size;
	unsigned long port1;	/* Port usually base+0 */
	unsigned long port2;	/* Port usually at base+0x400 */
	unsigned long port3;	/* Port usually at base+0x800 */
	unsigned short last_reg;/* Last register command */
	spinlock_t reg_lock;

	int dram_checked;

	snd_card_t *card;		/* The card that this belongs to */

	int chorus_mode;
	int reverb_mode;
	int bass_level;
	int treble_level;

	snd_emux_memhdr_t *memhdr;

	spinlock_t control_lock;
	snd_kcontrol_t * control_bass;
	snd_kcontrol_t * control_treble;

} emu8000_t;

/* sequencer device id */
#define SND_SEQ_DEV_ID_EMU8000	"synth-emu8000"

/* argument for snd_seq_device_new */
typedef struct emu8000_arg {
	int port;		/* base i/o port */
	int index;		/* sequencer client index */
	int seq_ports;		/* number of sequencer ports to be created */
} emu8000_arg_t;

#endif
