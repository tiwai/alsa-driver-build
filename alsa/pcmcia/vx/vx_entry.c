/*
 * Driver for Digigram VXpocket soundcards
 *
 * PCMCIA entry part
 *
 * Copyright (c) 2002 by Takashi Iwai <tiwai@suse.de>
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
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <sound/driver.h>
#include <sound/core.h>
#include "vxpocket.h"
#include <pcmcia/ciscode.h>
#include <pcmcia/cisreg.h>


/*
 * prototypes
 */
static void vxpocket_config(dev_link_t *link);
static void vxpocket_release(u_long arg);
static int vxpocket_event(event_t event, int priority, event_callback_args_t *args);
static void vx_card_free_callback(snd_card_t *card);


/*
 * flush the stale links.
 * the last instances might be left unreleased, so we release now before
 * creating a new one.
 */
static void flush_stale_links(struct snd_vxp_entry *hw)
{
	dev_link_t *link, *next;
	for (link = hw->dev_list; link; link = next) {
		next = link->next;
		if (link->state & DEV_STALE_LINK)
			snd_vxpocket_detach(hw ,link);
	}
}

/*
 * print the error message related with cs
 */
static void cs_error(client_handle_t handle, int func, int ret)
{
	error_info_t err = { func, ret };
	CardServices(ReportError, handle, &err);
}

/*
 * snd_vxpocket_attach - attach callback for cs
 * @hw: the hardware information
 */
dev_link_t *snd_vxpocket_attach(struct snd_vxp_entry *hw)
{
	client_reg_t client_reg;	/* Register with cardmgr */
	dev_link_t *link;		/* Info for cardmgr */
	int i, ret;
	vxpocket_t *chip;

	flush_stale_links(hw);

	chip = snd_vxpocket_create_chip(hw);
	if (! chip)
		return NULL;
	chip->card->private_free = vx_card_free_callback;

	link = &chip->link;
	link->priv = chip;

	link->release.function = &vxpocket_release;
	link->release.data = (u_long)link;

	link->io.Attributes1 = IO_DATA_PATH_WIDTH_AUTO;
	link->io.NumPorts1 = 16;

	link->irq.Attributes = IRQ_TYPE_EXCLUSIVE | IRQ_HANDLE_PRESENT;
	// link->irq.Attributes = IRQ_TYPE_DYNAMIC_SHARING|IRQ_FIRST_SHARED;

	link->irq.IRQInfo1 = IRQ_INFO2_VALID | IRQ_LEVEL_ID;
	if (hw->irq_list[0] == -1)
		link->irq.IRQInfo2 = *hw->irq_mask_p;
	else
		for (i = 0; i < 4; i++)
			link->irq.IRQInfo2 |= 1 << hw->irq_list[i];
	link->irq.Handler = &snd_vx_irq_handler;
	link->irq.Instance = chip;

	link->conf.Attributes = CONF_ENABLE_IRQ;
	link->conf.Vcc = 50;
	link->conf.IntType = INT_MEMORY_AND_IO;
	link->conf.ConfigIndex = 1;
	link->conf.Present = PRESENT_OPTION;

	/* Chain drivers */
	link->next = hw->dev_list;
	hw->dev_list = link;

	/* Register with Card Services */
	client_reg.dev_info = hw->dev_info;
	client_reg.Attributes = INFO_IO_CLIENT | INFO_CARD_SHARE;
	client_reg.EventMask = 
		CS_EVENT_CARD_INSERTION | CS_EVENT_CARD_REMOVAL |
		CS_EVENT_RESET_PHYSICAL | CS_EVENT_CARD_RESET |
		CS_EVENT_PM_SUSPEND | CS_EVENT_PM_RESUME;
	client_reg.event_handler = &vxpocket_event;
	client_reg.Version = 0x0210;
	client_reg.event_callback_args.client_data = link;

	ret = CardServices(RegisterClient, &link->handle, &client_reg);
	if (ret != CS_SUCCESS) {
		cs_error(link->handle, RegisterClient, ret);
		snd_vxpocket_detach(hw, link);
		return NULL;
	}

	return link;
}


/*
 */
static void vx_card_free_callback(snd_card_t *card)
{
	vxpocket_t *chip = snd_magic_cast(vxpocket_t, card->private_data, return);

	if (chip->link.state & DEV_CONFIG)
		vxpocket_release((u_long) &chip->link);
	snd_vxpocket_free_chip(chip);
	snd_magic_kfree(chip);
}

/*
 * snd_vxpocket_detach - detach callback for cs
 * @hw: the hardware information
 */
void snd_vxpocket_detach(struct snd_vxp_entry *hw, dev_link_t *link)
{
	dev_link_t **linkp;
	vxpocket_t *chip = snd_magic_cast(vxpocket_t, link->priv, return);

	/* Locate device structure */
	for (linkp = &hw->dev_list; *linkp; linkp = &(*linkp)->next)
		if (*linkp == link)
			break;
	if (*linkp == NULL)
		return;

	del_timer(&link->release);

	/* Break the link with Card Services */
	if (link->handle)
		CardServices(DeregisterClient, link->handle);
    
	/* Remove the interface data from the linked list */
	*linkp = link->next;

	chip->is_stale = 1;
	snd_card_disconnect(chip->card);
	snd_card_free_in_thread(chip->card);
}

/*
 * snd_vxpocket_detach_all - detach all instances linked to the hw
 */
void snd_vxpocket_detach_all(struct snd_vxp_entry *hw)
{
	while (hw->dev_list != NULL)
		snd_vxpocket_detach(hw, hw->dev_list);
}


/*
 * configuration callback
 */

#define CS_CHECK(fn, args...) \
while ((last_ret=CardServices(last_fn=(fn), args))!=0) goto cs_failed

static void vxpocket_config(dev_link_t *link)
{
	client_handle_t handle = link->handle;
	vxpocket_t *chip = snd_magic_cast(vxpocket_t, link->priv, return);
	tuple_t tuple;
	cisparse_t parse;
	u_short buf[32];
	int last_fn, last_ret;

	tuple.DesiredTuple = CISTPL_CFTABLE_ENTRY;
	tuple.Attributes = 0;
	tuple.TupleData = (cisdata_t *)buf;
	tuple.TupleDataMax = sizeof(buf);
	tuple.TupleOffset = 0;
	tuple.DesiredTuple = CISTPL_CONFIG;
	CS_CHECK(GetFirstTuple, handle, &tuple);
	CS_CHECK(GetTupleData, handle, &tuple);
	CS_CHECK(ParseTuple, handle, &tuple, &parse);
	link->conf.ConfigBase = parse.config.base;
	link->conf.ConfigIndex = 1;
	//link->conf.Present = parse.config.rmask[0];

	/* Configure card */
	link->state |= DEV_CONFIG;

	CS_CHECK(RequestIO, handle, &link->io);
	CS_CHECK(RequestIRQ, link->handle, &link->irq);
	CS_CHECK(RequestConfiguration, link->handle, &link->conf);

	if (snd_vxpocket_assign_resources(chip, link->io.BasePort1, link->irq.AssignedIRQ) < 0)
		goto failed;

	link->dev = &chip->node;
	link->state &= ~DEV_CONFIG_PENDING;
	return;

cs_failed:
	cs_error(link->handle, last_fn, last_ret);
failed:
	vxpocket_release((u_long)link);
}


/*
 * release callback
 */
static void vxpocket_release(u_long arg)
{
	dev_link_t *link = (dev_link_t *)arg;

	CardServices(ReleaseConfiguration, link->handle);
	CardServices(ReleaseIO, link->handle, &link->io);
	CardServices(ReleaseIRQ, link->handle, &link->irq);

	link->state &= ~DEV_CONFIG;
}


/*
 * event callback
 */
static int vxpocket_event(event_t event, int priority, event_callback_args_t *args)
{
	dev_link_t *link = args->client_data;
	vxpocket_t *chip = link->priv;

	switch (event) {
	case CS_EVENT_CARD_REMOVAL:
		link->state &= ~DEV_PRESENT;
		if (link->state & DEV_CONFIG) {
			mod_timer(&link->release, jiffies + HZ/20);
			if (chip) {
				chip->is_stale = 1;
				snd_card_disconnect(chip->card);
				snd_card_free_in_thread(chip->card);
			}
		}
		break;
	case CS_EVENT_CARD_INSERTION:
		link->state |= DEV_PRESENT;
		vxpocket_config(link);
		break;
	case CS_EVENT_PM_SUSPEND:
		link->state |= DEV_SUSPEND;
		if (chip)
			chip->in_suspend = 1;
		/* Fall through... */
	case CS_EVENT_RESET_PHYSICAL:
		if (link->state & DEV_CONFIG)
			CardServices(ReleaseConfiguration, link->handle);
		break;
	case CS_EVENT_PM_RESUME:
		link->state &= ~DEV_SUSPEND;
		/* Fall through... */
	case CS_EVENT_CARD_RESET:
		if (DEV_OK(link))
			CardServices(RequestConfiguration, link->handle, &link->conf);
		if (chip) {
			/* FIXME: call resume... */
			chip->in_suspend = 0;
		}
		break;
	}
	return 0;
}
