/*
 *  Advanced Linux Sound Architecture
 *
 *  InterWave FFFF Instrument Format
 *  Copyright (c) 1994-98 by Jaroslav Kysela <perex@suse.cz>
 *
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
 *
 */

#ifndef __AINSTR_IW_H
#define __AINSTR_IW_H

#ifndef __KERNEL__
#include <asm/types.h>
#endif

/*
 *  share types (share ID 1)
 */

#define IWFFFF_SHARE_FILE		0

/*
 *  wave formats
 */

#define IWFFFF_WAVE_16BIT		0x0001  /* 16-bit wave */
#define IWFFFF_WAVE_UNSIGNED		0x0002  /* unsigned wave */
#define IWFFFF_WAVE_INVERT		0x0002  /* same as unsigned wave */
#define IWFFFF_WAVE_BACKWARD		0x0004  /* backward mode (maybe used for reverb or ping-ping loop) */
#define IWFFFF_WAVE_LOOP		0x0008  /* loop mode */
#define IWFFFF_WAVE_BIDIR		0x0010  /* bidirectional mode */
#define IWFFFF_WAVE_ULAW		0x0020  /* uLaw compressed wave */
#define IWFFFF_WAVE_RAM			0x0040  /* wave is _preloaded_ in RAM (it is used for ROM simulation) */
#define IWFFFF_WAVE_ROM			0x0080  /* wave is in ROM */

/*
 *  Wavetable definitions
 */

typedef struct iwffff_wave {
	unsigned int share_id1;		/* share id (part 1) - 0 = no sharing */
	unsigned int share_id2;		/* share id (part 2) - 0 = no sharing */
	unsigned int share_id3;		/* share id (part 3) - 0 = no sharing */
	unsigned int share_id4;		/* share id (part 4) - 0 = no sharing */

	unsigned int format;		/* wave format */

	struct {
		unsigned int number;	/* some other ID for this instrument */
		unsigned int memory;	/* begin of waveform in onboard memory */
		unsigned char *ptr;	/* pointer to waveform in system memory */
	} address;

	unsigned int size;		/* size of waveform in bytes */
	unsigned int start;		/* start offset in bytes * 16 (lowest 4 bits - fraction) */
	unsigned int loop_start;	/* bits loop start offset in bytes * 16 (lowest 4 bits - fraction) */
	unsigned int loop_end;		/* loop start offset in bytes * 16 (lowest 4 bits - fraction) */
	unsigned short loop_repeat;	/* loop repeat - 0 = forever */
	unsigned int sample_ratio;	/* sample ratio (44100 * 1024 / rate) */
	unsigned char attenuation;	/* 0 - 127 (no corresponding midi controller) */
	unsigned char low_note;		/* lower frequency range for this waveform */
	unsigned char high_note;	/* highter frequency range for this waveform */
	unsigned char pad;
  
	struct iwffff_wave *next;
} iwffff_wave_t;

/*
 *  Layer
 */

#define IWFFFF_LFO_SHAPE_TRIANGLE	0
#define IWFFFF_LFO_SHAPE_POSTRIANGLE	1

typedef struct iwffff_lfo {
	unsigned short freq;		/* (0-2047) 0.01Hz - 21.5Hz */
	signed short depth;		/* volume +- (0-255) 0.48675dB/step */
	signed short sweep;		/* 0 - 950 deciseconds */
	unsigned char shape;		/* see to ULTRA_IW_LFO_SHAPE_XXXX */
	unsigned char delay;		/* 0 - 255 deciseconds */
} iwffff_lfo_t;

#define IWFFFF_ENV_FLAG_RETRIGGER	0x0001	/* flag - retrigger */

#define IWFFFF_ENV_MODE_ONE_SHOT	0x0001	/* mode - one shot */
#define IWFFFF_ENV_MODE_SUSTAIN		0x0002	/* mode - sustain */
#define IWFFFF_ENV_MODE_NO_SUSTAIN	0x0003	/* mode - no sustain */

#define IWFFFF_ENV_INDEX_VELOCITY	0x0001	/* index - velocity */
#define IWFFFF_ENV_INDEX_FREQUENCY	0x0002	/* index - frequency */

typedef struct iwffff_env_point {
	unsigned short offset;
	unsigned short rate;
} iwffff_env_point_t;

typedef struct iwffff_env_record {
	unsigned short nattack;
	unsigned short nrelease;
	unsigned short sustain_offset;
	unsigned short sustain_rate;
	unsigned short release_rate;
	unsigned char hirange;
	unsigned char pad;
	struct iwffff_env_record *next;
	/* points are stored here */
	/* count of points = nattack + nrelease */
} iwffff_env_record_t;

typedef struct iwffff_env {
	unsigned char flags;
  	unsigned char mode;
  	unsigned char index;
	unsigned char pad;
	struct iwffff_env_record *record;
} iwffff_env_t;

#define IWFFFF_LAYER_FLAG_RETRIGGER	0x0001	/* retrigger */

#define IWFFFF_LAYER_VELOCITY_TIME	0x0000	/* velocity mode = time */
#define IWFFFF_LAYER_VELOCITY_RATE	0x0001	/* velocity mode = rate */

#define IWFFFF_LAYER_EVENT_KUP		0x0000	/* layer event - key up */
#define IWFFFF_LAYER_EVENT_KDOWN	0x0001	/* layer event - key down */
#define IWFFFF_LAYER_EVENT_RETRIG	0x0002	/* layer event - retrigger */
#define IWFFFF_LAYER_EVENT_LEGATO	0x0003	/* layer event - legato */

typedef struct iwffff_layer {
	unsigned char flags;
	unsigned char velocity_mode;
      	unsigned char layer_event;
	unsigned char low_range;	/* range for layer based */
	unsigned char high_range;	/* on either velocity or frequency */
	unsigned char pan;		/* pan offset from CC1 (0 left - 127 right) */
	unsigned char pan_freq_scale;	/* position based on frequency (0-127) */
	unsigned char attenuation;	/* 0-127 (no corresponding midi controller) */
	iwffff_lfo_t tremolo;		/* tremolo effect */
	iwffff_lfo_t vibrato;		/* vibrato effect */
	unsigned short freq_scale;	/* 0-2048, 1024 is equal to semitone scaling */
	unsigned char freq_center;	/* center for keyboard frequency scaling */
	unsigned char pad;
	iwffff_env_t penv;		/* pitch envelope */
	iwffff_env_t venv;		/* volume envelope */

	iwffff_wave_t *wave;
	struct iwffff_layer *next;
} iwffff_layer_t;

/*
 *  Instrument
 */

#define IWFFFF_EXCLUDE_NONE		0x0000	/* exclusion mode - none */
#define IWFFFF_EXCLUDE_SINGLE		0x0001	/* exclude single - single note from the instrument group */
#define IWFFFF_EXCLUDE_MULTIPLE		0x0002	/* exclude multiple - stop only same note from this instrument */

#define IWFFFF_LAYER_NONE		0x0000	/* not layered */
#define IWFFFF_LAYER_ON			0x0001	/* layered */
#define IWFFFF_LAYER_VELOCITY		0x0002	/* layered by velocity */
#define IWFFFF_LAYER_FREQUENCY		0x0003	/* layered by frequency */

#define IWFFFF_EFFECT_NONE		0
#define IWFFFF_EFFECT_REVERB		1
#define IWFFFF_EFFECT_CHORUS		2
#define IWFFFF_EFFECT_ECHO		3

typedef struct {
	unsigned short exclusion;
	unsigned short layer_type;
	unsigned short exclusion_group;	/* 0 - none, 1-65535 */

	unsigned char effect1;		/* effect 1 */
	unsigned char effect1_depth;	/* 0-127 */
	unsigned char effect2;		/* effect 2 */
	unsigned char effect2_depth;	/* 0-127 */

	iwffff_layer_t *layer;		/* first layer */
} iwffff_instrument_t;

/*
 *
 *    Kernel <-> user space
 *    Hardware (CPU) independent section
 *
 *    * = zero or more
 *    + = one or more
 *
 *    iwffff_xinstrument		IWFFFF_STRU_INSTR
 *      +iwffff_xlayer			IWFFFF_STRU_LAYER
 *        *iwffff_xenv_record		IWFFFF_STRU_ENV_RECT (tremolo)
 *        *iwffff_xenv_record		IWFFFF_STRU_EVN_RECT (vibrato)
 *          +iwffff_xwave		IWFFFF_STRU_WAVE
 *
 */

#define IWFFFF_STRU_WAVE	snd_htoi_32(('W'<<24)|('A'<<16)|('V'<<8)|'E')
#define IWFFFF_STRU_ENV_RECP	snd_htoi_32(('E'<<24)|('N'<<16)|('R'<<8)|'P')
#define IWFFFF_STRU_ENV_RECV	snd_htoi_32(('E'<<24)|('N'<<16)|('R'<<8)|'V')
#define IWFFFF_STRU_LAYER 	snd_htoi_32(('L'<<24)|('A'<<16)|('Y'<<8)|'R')
#define IWFFFF_STRU_INSTR 	snd_htoi_32(('I'<<24)|('N'<<16)|('S'<<8)|'T')

/*
 *  Wavetable definitions
 */

typedef struct iwffff_xwave {
	__u32 stype;			/* structure type */

	__u32 share_id1;		/* share id (part 1) - 0 = no sharing */
	__u32 share_id2;		/* share id (part 2) - 0 = no sharing */
	__u32 share_id3;		/* share id (part 3) - 0 = no sharing */
	__u32 share_id4;		/* share id (part 4) - 0 = no sharing */

	__u32 format;			/* wave format */
	__u32 offset;			/* offset to ROM (address) */

	__u32 size;			/* size of waveform in bytes */
	__u32 start;			/* start offset in bytes * 16 (lowest 4 bits - fraction) */
	__u32 loop_start;		/* bits loop start offset in bytes * 16 (lowest 4 bits - fraction) */
	__u32 loop_end;			/* loop start offset in bytes * 16 (lowest 4 bits - fraction) */
	__u16 loop_repeat;		/* loop repeat - 0 = forever */
	__u32 sample_ratio;		/* sample ratio (44100 * 1024 / rate) */
	__u8 attenuation;		/* 0 - 127 (no corresponding midi controller) */
	__u8 low_note;			/* lower frequency range for this waveform */
	__u8 high_note;			/* highter frequency range for this waveform */
	__u8 pad;
} iwffff_xwave_t;

/*
 *  Layer
 */

typedef struct iwffff_xlfo {
	__u16 freq;			/* (0-2047) 0.01Hz - 21.5Hz */
	__s16 depth;			/* volume +- (0-255) 0.48675dB/step */
	__s16 sweep;			/* 0 - 950 deciseconds */
	__u8 shape;			/* see to ULTRA_IW_LFO_SHAPE_XXXX */
	__u8 delay;			/* 0 - 255 deciseconds */
} iwffff_xlfo_t;

typedef struct iwffff_xenv_point {
	__u16 offset;
	__u16 rate;
} iwffff_xenv_point_t;

typedef struct iwffff_xenv_record {
	__u32 stype;
	__u16 nattack;
	__u16 nrelease;
	__u16 sustain_offset;
	__u16 sustain_rate;
	__u16 release_rate;
	__u8 hirange;
	__u8 pad;
	/* points are stored here.. */
	/* count of points = nattack + nrelease */
} iwffff_xenv_record_t;

typedef struct iwffff_xenv {
	__u8 flags;
  	__u8 mode;
  	__u8 index;
	__u8 pad;
} iwffff_xenv_t;

typedef struct iwffff_xlayer {
	__u32 stype;
	__u8 flags;
	__u8 velocity_mode;
      	__u8 layer_event;
	__u8 low_range;			/* range for layer based */
	__u8 high_range;		/* on either velocity or frequency */
	__u8 pan;			/* pan offset from CC1 (0 left - 127 right) */
	__u8 pan_freq_scale;		/* position based on frequency (0-127) */
	__u8 attenuation;		/* 0-127 (no corresponding midi controller) */
	iwffff_xlfo_t tremolo;		/* tremolo effect */
	iwffff_xlfo_t vibrato;		/* vibrato effect */
	__u16 freq_scale;		/* 0-2048, 1024 is equal to semitone scaling */
	__u8 freq_center;		/* center for keyboard frequency scaling */
	__u8 pad;
	iwffff_xenv_t penv;		/* pitch envelope */
	iwffff_xenv_t venv;		/* volume envelope */
} iwffff_xlayer_t;

/*
 *  Instrument
 */

typedef struct iwffff_xinstrument {
	__u32 stype;
	
	__u16 exclusion;
	__u16 layer_type;
	__u16 exclusion_group;		/* 0 - none, 1-65535 */

	__u8 effect1;			/* effect 1 */
	__u8 effect1_depth;		/* 0-127 */
	__u8 effect2;			/* effect 2 */
	__u8 effect2_depth;		/* 0-127 */
} iwffff_xinstrument_t;

/*
 *  ROM support
 *    InterWave ROMs are Little-Endian (x86)
 */

#define IWFFFF_ROM_HDR_SIZE	512

typedef struct {
	__u8 iwave[8];
	__u8 revision;
	__u8 series_number;
	__u8 series_name[16];
	__u8 date[10];
	__u16 vendor_revision_major;
	__u16 vendor_revision_minor;
	__u32 rom_size;
	__u8 copyright[128];
	__u8 vendor_name[64];
	__u8 description[128];
} iwffff_rom_header_t;

#endif /* __SEQ_INSTR_IW_H */
