/*
 *  Copyright (C) 2000 Takashi Iwai <tiwai@suse.de>
 *
 *  Routines for control of EMU WaveTable chip
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

#include <sound/emux_synth.h>
#include "emux_voice.h"

MODULE_AUTHOR("Takashi Iwai");
MODULE_DESCRIPTION("Routines for control of EMU WaveTable chip");
MODULE_LICENSE("GPL");

/*
 * create a new hardware dependent device for Emu8000/Emu10k1
 */
int snd_emux_new(snd_emux_t **remu)
{
	snd_emux_t *emu;

	*remu = NULL;
	emu = snd_magic_kcalloc(snd_emux_t, 0, GFP_KERNEL);
	if (emu == NULL)
		return -ENOMEM;

	spin_lock_init(&emu->voice_lock);
	init_MUTEX(&emu->register_mutex);

	emu->client = -1;
#ifdef CONFIG_SND_OSSEMUL
	emu->oss_synth = NULL;
#endif
	emu->max_voices = 0;
	emu->use_time = 0;

	*remu = emu;
	return 0;
}


/*
 */
int snd_emux_register(snd_emux_t *emu, snd_card_t *card, int index, char *name)
{
	snd_sf_callback_t sf_cb;

	snd_assert(emu->hw != NULL, return -EINVAL);
	snd_assert(emu->max_voices > 0, return -EINVAL);
	snd_assert(card != NULL, return -EINVAL);
	snd_assert(name != NULL, return -EINVAL);

	emu->card = card;
	emu->name = snd_kmalloc_strdup(name, GFP_KERNEL);
	emu->voices = snd_kcalloc(sizeof(snd_emux_voice_t) * emu->max_voices, GFP_KERNEL);
	if (emu->voices == NULL)
		return -ENOMEM;

	/* create soundfont list */
	memset(&sf_cb, 0, sizeof(sf_cb));
	sf_cb.private_data = emu;
	sf_cb.sample_new = (snd_sf_sample_new_t)emu->ops.sample_new;
	sf_cb.sample_free = (snd_sf_sample_free_t)emu->ops.sample_free;
	sf_cb.sample_reset = (snd_sf_sample_reset_t)emu->ops.sample_reset;
	emu->sflist = snd_sf_new(&sf_cb, emu->memhdr);
	if (emu->sflist == NULL)
		return -ENOMEM;

	snd_emux_init_voices(emu);

	snd_emux_init_seq(emu, card, index);
#ifdef CONFIG_SND_OSSEMUL
	snd_emux_init_seq_oss(emu);
#endif
	snd_emux_init_virmidi(emu, card);

#ifdef CONFIG_PROC_FS
	snd_emux_proc_init(emu, card, index);
#endif
	return 0;
}


/*
 */
int snd_emux_free(snd_emux_t *emu)
{
	if (! emu)
		return -EINVAL;

#ifdef CONFIG_PROC_FS
	snd_emux_proc_free(emu);
#endif
	snd_emux_delete_virmidi(emu);
#ifdef CONFIG_SND_OSSEMUL
	snd_emux_detach_seq_oss(emu);
#endif
	snd_emux_detach_seq(emu);

	if (emu->sflist)
		snd_sf_free(emu->sflist);

	if (emu->voices)
		kfree(emu->voices);

	if (emu->name)
		kfree(emu->name);

	snd_magic_kfree(emu);
	return 0;
}


EXPORT_SYMBOL(snd_emux_new);
EXPORT_SYMBOL(snd_emux_register);
EXPORT_SYMBOL(snd_emux_free);

EXPORT_SYMBOL(snd_emux_terminate_all);
EXPORT_SYMBOL(snd_emux_lock_voice);
EXPORT_SYMBOL(snd_emux_unlock_voice);


/*
 *  INIT part
 */

static int __init alsa_emux_init(void)
{
	return 0;
}

static void __exit alsa_emux_exit(void)
{
}

module_init(alsa_emux_init)
module_exit(alsa_emux_exit)
