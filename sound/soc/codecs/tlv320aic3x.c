/*
 * ALSA SoC TLV320AIC3X codec driver
 *
 * Author:      Vladimir Barinov, <vbarinov@ru.mvista.com>
 * Copyright:   (C) 2007 MontaVista Software, Inc., <source@mvista.com>
 *
 * Based on sound/soc/codecs/wm8753.c by Liam Girdwood
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Notes:
 *  The AIC3X is a driver for a low power stereo audio
 *  codecs aic31, aic32, aic33.
 *
 *  It supports full aic33 codec functionality.
 *  The compatibility with aic32, aic31 is as follows:
 *        aic32        |        aic31
 *  ---------------------------------------
 *   MONO_LOUT -> N/A  |  MONO_LOUT -> N/A
 *                     |  IN1L -> LINE1L
 *                     |  IN1R -> LINE1R
 *                     |  IN2L -> LINE2L
 *                     |  IN2R -> LINE2R
 *                     |  MIC3L/R -> N/A
 *   truncated internal functionality in
 *   accordance with documentation
 *  ---------------------------------------
 *
 *  Hence the machine layer should disable unsupported inputs/outputs by
 *  snd_soc_dapm_set_endpoint(codec, "MONO_LOUT", 0), etc.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>

#include "tlv320aic3x.h"

#define AUDIO_NAME "aic3x"
#define AIC3X_VERSION "0.2"

/* codec private data */
struct aic3x_priv {
	unsigned int sysclk;
	int master;
};

/*
 * AIC3X register cache
 * We can't read the AIC3X register space when we are
 * using 2 wire for device control, so we cache them instead.
 * There is no point in caching the reset register
 */
static const u8 aic3x_reg[AIC3X_CACHEREGNUM] = {
	0x00, 0x00, 0x00, 0x10,	/* 0 */
	0x04, 0x00, 0x00, 0x00,	/* 4 */
	0x00, 0x00, 0x00, 0x01,	/* 8 */
	0x00, 0x00, 0x00, 0x80,	/* 12 */
	0x80, 0xff, 0xff, 0x78,	/* 16 */
	0x78, 0x78, 0x78, 0x78,	/* 20 */
	0x78, 0x00, 0x00, 0xfe,	/* 24 */
	0x00, 0x00, 0xfe, 0x00,	/* 28 */
	0x18, 0x18, 0x00, 0x00,	/* 32 */
	0x00, 0x00, 0x00, 0x00,	/* 36 */
	0x00, 0x00, 0x00, 0x80,	/* 40 */
	0x80, 0x00, 0x00, 0x00,	/* 44 */
	0x00, 0x00, 0x00, 0x04,	/* 48 */
	0x00, 0x00, 0x00, 0x00,	/* 52 */
	0x00, 0x00, 0x04, 0x00,	/* 56 */
	0x00, 0x00, 0x00, 0x00,	/* 60 */
	0x00, 0x04, 0x00, 0x00,	/* 64 */
	0x00, 0x00, 0x00, 0x00,	/* 68 */
	0x04, 0x00, 0x00, 0x00,	/* 72 */
	0x00, 0x00, 0x00, 0x00,	/* 76 */
	0x00, 0x00, 0x00, 0x00,	/* 80 */
	0x00, 0x00, 0x00, 0x00,	/* 84 */
	0x00, 0x00, 0x00, 0x00,	/* 88 */
	0x00, 0x00, 0x00, 0x00,	/* 92 */
	0x00, 0x00, 0x00, 0x00,	/* 96 */
	0x00, 0x00, 0x02,	/* 100 */
};

/*
 * read aic3x register cache
 */
static inline unsigned int aic3x_read_reg_cache(struct snd_soc_codec *codec,
						unsigned int reg)
{
	u8 *cache = codec->reg_cache;
	if (reg >= AIC3X_CACHEREGNUM)
		return -1;
	return cache[reg];
}

/*
 * write aic3x register cache
 */
static inline void aic3x_write_reg_cache(struct snd_soc_codec *codec,
					 u8 reg, u8 value)
{
	u8 *cache = codec->reg_cache;
	if (reg >= AIC3X_CACHEREGNUM)
		return;
	cache[reg] = value;
}

/*
 * write to the aic3x register space
 */
static int aic3x_write(struct snd_soc_codec *codec, unsigned int reg,
		       unsigned int value)
{
	u8 data[2];

	/* data is
	 *   D15..D8 aic3x register offset
	 *   D7...D0 register data
	 */
	data[0] = reg & 0xff;
	data[1] = value & 0xff;

	aic3x_write_reg_cache(codec, data[0], data[1]);
	if (codec->hw_write(codec->control_data, data, 2) == 2)
		return 0;
	else
		return -EIO;
}

/*
 * read from the aic3x register space
 */
static int aic3x_read(struct snd_soc_codec *codec, unsigned int reg,
		      u8 *value)
{
	*value = reg & 0xff;
	if (codec->hw_read(codec->control_data, value, 1) != 1)
		return -EIO;

	aic3x_write_reg_cache(codec, reg, *value);
	return 0;
}

#define SOC_DAPM_SINGLE_AIC3X(xname, reg, shift, mask, invert) \
{	.iface = SNDRV_CTL_ELEM_IFACE_MIXER, .name = xname, \
	.info = snd_soc_info_volsw, \
	.get = snd_soc_dapm_get_volsw, .put = snd_soc_dapm_put_volsw_aic3x, \
	.private_value =  SOC_SINGLE_VALUE(reg, shift, mask, invert) }

/*
 * All input lines are connected when !0xf and disconnected with 0xf bit field,
 * so we have to use specific dapm_put call for input mixer
 */
static int snd_soc_dapm_put_volsw_aic3x(struct snd_kcontrol *kcontrol,
					struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_dapm_widget *widget = snd_kcontrol_chip(kcontrol);
	int reg = kcontrol->private_value & 0xff;
	int shift = (kcontrol->private_value >> 8) & 0x0f;
	int mask = (kcontrol->private_value >> 16) & 0xff;
	int invert = (kcontrol->private_value >> 24) & 0x01;
	unsigned short val, val_mask;
	int ret;
	struct snd_soc_dapm_path *path;
	int found = 0;

	val = (ucontrol->value.integer.value[0] & mask);

	mask = 0xf;
	if (val)
		val = mask;

	if (invert)
		val = mask - val;
	val_mask = mask << shift;
	val = val << shift;

	mutex_lock(&widget->codec->mutex);

	if (snd_soc_test_bits(widget->codec, reg, val_mask, val)) {
		/* find dapm widget path assoc with kcontrol */
		list_for_each_entry(path, &widget->codec->dapm_paths, list) {
			if (path->kcontrol != kcontrol)
				continue;

			/* found, now check type */
			found = 1;
			if (val)
				/* new connection */
				path->connect = invert ? 0 : 1;
			else
				/* old connection must be powered down */
				path->connect = invert ? 1 : 0;
			break;
		}

		if (found)
			snd_soc_dapm_sync_endpoints(widget->codec);
	}

	ret = snd_soc_update_bits(widget->codec, reg, val_mask, val);

	mutex_unlock(&widget->codec->mutex);
	return ret;
}

static const char *aic3x_left_dac_mux[] = { "DAC_L1", "DAC_L3", "DAC_L2" };
static const char *aic3x_right_dac_mux[] = { "DAC_R1", "DAC_R3", "DAC_R2" };
static const char *aic3x_left_hpcom_mux[] =
    { "differential of HPLOUT", "constant VCM", "single-ended" };
static const char *aic3x_right_hpcom_mux[] =
    { "differential of HPROUT", "constant VCM", "single-ended",
      "differential of HPLCOM", "external feedback" };
static const char *aic3x_linein_mode_mux[] = { "single-ended", "differential" };

#define LDAC_ENUM	0
#define RDAC_ENUM	1
#define LHPCOM_ENUM	2
#define RHPCOM_ENUM	3
#define LINE1L_ENUM	4
#define LINE1R_ENUM	5
#define LINE2L_ENUM	6
#define LINE2R_ENUM	7

static const struct soc_enum aic3x_enum[] = {
	SOC_ENUM_SINGLE(DAC_LINE_MUX, 6, 3, aic3x_left_dac_mux),
	SOC_ENUM_SINGLE(DAC_LINE_MUX, 4, 3, aic3x_right_dac_mux),
	SOC_ENUM_SINGLE(HPLCOM_CFG, 4, 3, aic3x_left_hpcom_mux),
	SOC_ENUM_SINGLE(HPRCOM_CFG, 3, 5, aic3x_right_hpcom_mux),
	SOC_ENUM_SINGLE(LINE1L_2_LADC_CTRL, 7, 2, aic3x_linein_mode_mux),
	SOC_ENUM_SINGLE(LINE1R_2_RADC_CTRL, 7, 2, aic3x_linein_mode_mux),
	SOC_ENUM_SINGLE(LINE2L_2_LADC_CTRL, 7, 2, aic3x_linein_mode_mux),
	SOC_ENUM_SINGLE(LINE2R_2_RADC_CTRL, 7, 2, aic3x_linein_mode_mux),
};

static const struct snd_kcontrol_new aic3x_snd_controls[] = {
	/* Output */
	SOC_DOUBLE_R("PCM Playback Volume", LDAC_VOL, RDAC_VOL, 0, 0x7f, 1),

	SOC_DOUBLE_R("Line DAC Playback Volume", DACL1_2_LLOPM_VOL,
		     DACR1_2_RLOPM_VOL, 0, 0x7f, 1),
	SOC_DOUBLE_R("Line DAC Playback Switch", LLOPM_CTRL, RLOPM_CTRL, 3,
		     0x01, 0),
	SOC_DOUBLE_R("Line PGA Bypass Playback Volume", PGAL_2_LLOPM_VOL,
		     PGAR_2_RLOPM_VOL, 0, 0x7f, 1),
	SOC_DOUBLE_R("Line Line2 Bypass Playback Volume", LINE2L_2_LLOPM_VOL,
		     LINE2R_2_RLOPM_VOL, 0, 0x7f, 1),

	SOC_DOUBLE_R("Mono DAC Playback Volume", DACL1_2_MONOLOPM_VOL,
		     DACR1_2_MONOLOPM_VOL, 0, 0x7f, 1),
	SOC_SINGLE("Mono DAC Playback Switch", MONOLOPM_CTRL, 3, 0x01, 0),
	SOC_DOUBLE_R("Mono PGA Bypass Playback Volume", PGAL_2_MONOLOPM_VOL,
		     PGAR_2_MONOLOPM_VOL, 0, 0x7f, 1),
	SOC_DOUBLE_R("Mono Line2 Bypass Playback Volume", LINE2L_2_MONOLOPM_VOL,
		     LINE2R_2_MONOLOPM_VOL, 0, 0x7f, 1),

	SOC_DOUBLE_R("HP DAC Playback Volume", DACL1_2_HPLOUT_VOL,
		     DACR1_2_HPROUT_VOL, 0, 0x7f, 1),
	SOC_DOUBLE_R("HP DAC Playback Switch", HPLOUT_CTRL, HPROUT_CTRL, 3,
		     0x01, 0),
	SOC_DOUBLE_R("HP PGA Bypass Playback Volume", PGAL_2_HPLOUT_VOL,
		     PGAR_2_HPROUT_VOL, 0, 0x7f, 1),
	SOC_DOUBLE_R("HP Line2 Bypass Playback Volume", LINE2L_2_HPLOUT_VOL,
		     LINE2R_2_HPROUT_VOL, 0, 0x7f, 1),

	SOC_DOUBLE_R("HPCOM DAC Playback Volume", DACL1_2_HPLCOM_VOL,
		     DACR1_2_HPRCOM_VOL, 0, 0x7f, 1),
	SOC_DOUBLE_R("HPCOM DAC Playback Switch", HPLCOM_CTRL, HPRCOM_CTRL, 3,
		     0x01, 0),
	SOC_DOUBLE_R("HPCOM PGA Bypass Playback Volume", PGAL_2_HPLCOM_VOL,
		     PGAR_2_HPRCOM_VOL, 0, 0x7f, 1),
	SOC_DOUBLE_R("HPCOM Line2 Bypass Playback Volume", LINE2L_2_HPLCOM_VOL,
		     LINE2R_2_HPRCOM_VOL, 0, 0x7f, 1),

	/*
	 * Note: enable Automatic input Gain Controller with care. It can
	 * adjust PGA to max value when ADC is on and will never go back.
	*/
	SOC_DOUBLE_R("AGC Switch", LAGC_CTRL_A, RAGC_CTRL_A, 7, 0x01, 0),

	/* Input */
	SOC_DOUBLE_R("PGA Capture Volume", LADC_VOL, RADC_VOL, 0, 0x7f, 0),
	SOC_DOUBLE_R("PGA Capture Switch", LADC_VOL, RADC_VOL, 7, 0x01, 1),
};

/* add non dapm controls */
static int aic3x_add_controls(struct snd_soc_codec *codec)
{
	int err, i;

	for (i = 0; i < ARRAY_SIZE(aic3x_snd_controls); i++) {
		err = snd_ctl_add(codec->card,
				  snd_soc_cnew(&aic3x_snd_controls[i],
					       codec, NULL));
		if (err < 0)
			return err;
	}

	return 0;
}

/* Left DAC Mux */
static const struct snd_kcontrol_new aic3x_left_dac_mux_controls =
SOC_DAPM_ENUM("Route", aic3x_enum[LDAC_ENUM]);

/* Right DAC Mux */
static const struct snd_kcontrol_new aic3x_right_dac_mux_controls =
SOC_DAPM_ENUM("Route", aic3x_enum[RDAC_ENUM]);

/* Left HPCOM Mux */
static const struct snd_kcontrol_new aic3x_left_hpcom_mux_controls =
SOC_DAPM_ENUM("Route", aic3x_enum[LHPCOM_ENUM]);

/* Right HPCOM Mux */
static const struct snd_kcontrol_new aic3x_right_hpcom_mux_controls =
SOC_DAPM_ENUM("Route", aic3x_enum[RHPCOM_ENUM]);

/* Left DAC_L1 Mixer */
static const struct snd_kcontrol_new aic3x_left_dac_mixer_controls[] = {
	SOC_DAPM_SINGLE("Line Switch", DACL1_2_LLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Mono Switch", DACL1_2_MONOLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HP Switch", DACL1_2_HPLOUT_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HPCOM Switch", DACL1_2_HPLCOM_VOL, 7, 1, 0),
};

/* Right DAC_R1 Mixer */
static const struct snd_kcontrol_new aic3x_right_dac_mixer_controls[] = {
	SOC_DAPM_SINGLE("Line Switch", DACR1_2_RLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Mono Switch", DACR1_2_MONOLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HP Switch", DACR1_2_HPROUT_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HPCOM Switch", DACR1_2_HPRCOM_VOL, 7, 1, 0),
};

/* Left PGA Mixer */
static const struct snd_kcontrol_new aic3x_left_pga_mixer_controls[] = {
	SOC_DAPM_SINGLE_AIC3X("Line1L Switch", LINE1L_2_LADC_CTRL, 3, 1, 1),
	SOC_DAPM_SINGLE_AIC3X("Line2L Switch", LINE2L_2_LADC_CTRL, 3, 1, 1),
	SOC_DAPM_SINGLE_AIC3X("Mic3L Switch", MIC3LR_2_LADC_CTRL, 4, 1, 1),
};

/* Right PGA Mixer */
static const struct snd_kcontrol_new aic3x_right_pga_mixer_controls[] = {
	SOC_DAPM_SINGLE_AIC3X("Line1R Switch", LINE1R_2_RADC_CTRL, 3, 1, 1),
	SOC_DAPM_SINGLE_AIC3X("Line2R Switch", LINE2R_2_RADC_CTRL, 3, 1, 1),
	SOC_DAPM_SINGLE_AIC3X("Mic3R Switch", MIC3LR_2_RADC_CTRL, 0, 1, 1),
};

/* Left Line1 Mux */
static const struct snd_kcontrol_new aic3x_left_line1_mux_controls =
SOC_DAPM_ENUM("Route", aic3x_enum[LINE1L_ENUM]);

/* Right Line1 Mux */
static const struct snd_kcontrol_new aic3x_right_line1_mux_controls =
SOC_DAPM_ENUM("Route", aic3x_enum[LINE1R_ENUM]);

/* Left Line2 Mux */
static const struct snd_kcontrol_new aic3x_left_line2_mux_controls =
SOC_DAPM_ENUM("Route", aic3x_enum[LINE2L_ENUM]);

/* Right Line2 Mux */
static const struct snd_kcontrol_new aic3x_right_line2_mux_controls =
SOC_DAPM_ENUM("Route", aic3x_enum[LINE2R_ENUM]);

/* Left PGA Bypass Mixer */
static const struct snd_kcontrol_new aic3x_left_pga_bp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Line Switch", PGAL_2_LLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Mono Switch", PGAL_2_MONOLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HP Switch", PGAL_2_HPLOUT_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HPCOM Switch", PGAL_2_HPLCOM_VOL, 7, 1, 0),
};

/* Right PGA Bypass Mixer */
static const struct snd_kcontrol_new aic3x_right_pga_bp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Line Switch", PGAR_2_RLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Mono Switch", PGAR_2_MONOLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HP Switch", PGAR_2_HPROUT_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HPCOM Switch", PGAR_2_HPRCOM_VOL, 7, 1, 0),
};

/* Left Line2 Bypass Mixer */
static const struct snd_kcontrol_new aic3x_left_line2_bp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Line Switch", LINE2L_2_LLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Mono Switch", LINE2L_2_MONOLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HP Switch", LINE2L_2_HPLOUT_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HPCOM Switch", LINE2L_2_HPLCOM_VOL, 7, 1, 0),
};

/* Right Line2 Bypass Mixer */
static const struct snd_kcontrol_new aic3x_right_line2_bp_mixer_controls[] = {
	SOC_DAPM_SINGLE("Line Switch", LINE2R_2_RLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("Mono Switch", LINE2R_2_MONOLOPM_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HP Switch", LINE2R_2_HPROUT_VOL, 7, 1, 0),
	SOC_DAPM_SINGLE("HPCOM Switch", LINE2R_2_HPRCOM_VOL, 7, 1, 0),
};

static const struct snd_soc_dapm_widget aic3x_dapm_widgets[] = {
	/* Left DAC to Left Outputs */
	SND_SOC_DAPM_DAC("Left DAC", "Left Playback", DAC_PWR, 7, 0),
	SND_SOC_DAPM_MUX("Left DAC Mux", SND_SOC_NOPM, 0, 0,
			 &aic3x_left_dac_mux_controls),
	SND_SOC_DAPM_MIXER("Left DAC_L1 Mixer", SND_SOC_NOPM, 0, 0,
			   &aic3x_left_dac_mixer_controls[0],
			   ARRAY_SIZE(aic3x_left_dac_mixer_controls)),
	SND_SOC_DAPM_MUX("Left HPCOM Mux", SND_SOC_NOPM, 0, 0,
			 &aic3x_left_hpcom_mux_controls),
	SND_SOC_DAPM_PGA("Left Line Out", LLOPM_CTRL, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left HP Out", HPLOUT_CTRL, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Left HP Com", HPLCOM_CTRL, 0, 0, NULL, 0),

	/* Right DAC to Right Outputs */
	SND_SOC_DAPM_DAC("Right DAC", "Right Playback", DAC_PWR, 6, 0),
	SND_SOC_DAPM_MUX("Right DAC Mux", SND_SOC_NOPM, 0, 0,
			 &aic3x_right_dac_mux_controls),
	SND_SOC_DAPM_MIXER("Right DAC_R1 Mixer", SND_SOC_NOPM, 0, 0,
			   &aic3x_right_dac_mixer_controls[0],
			   ARRAY_SIZE(aic3x_right_dac_mixer_controls)),
	SND_SOC_DAPM_MUX("Right HPCOM Mux", SND_SOC_NOPM, 0, 0,
			 &aic3x_right_hpcom_mux_controls),
	SND_SOC_DAPM_PGA("Right Line Out", RLOPM_CTRL, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right HP Out", HPROUT_CTRL, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("Right HP Com", HPRCOM_CTRL, 0, 0, NULL, 0),

	/* Mono Output */
	SND_SOC_DAPM_PGA("Mono Out", MONOLOPM_CTRL, 0, 0, NULL, 0),

	/* Left Inputs to Left ADC */
	SND_SOC_DAPM_ADC("Left ADC", "Left Capture", LINE1L_2_LADC_CTRL, 2, 0),
	SND_SOC_DAPM_MIXER("Left PGA Mixer", SND_SOC_NOPM, 0, 0,
			   &aic3x_left_pga_mixer_controls[0],
			   ARRAY_SIZE(aic3x_left_pga_mixer_controls)),
	SND_SOC_DAPM_MUX("Left Line1L Mux", SND_SOC_NOPM, 0, 0,
			 &aic3x_left_line1_mux_controls),
	SND_SOC_DAPM_MUX("Left Line2L Mux", SND_SOC_NOPM, 0, 0,
			 &aic3x_left_line2_mux_controls),

	/* Right Inputs to Right ADC */
	SND_SOC_DAPM_ADC("Right ADC", "Right Capture",
			 LINE1R_2_RADC_CTRL, 2, 0),
	SND_SOC_DAPM_MIXER("Right PGA Mixer", SND_SOC_NOPM, 0, 0,
			   &aic3x_right_pga_mixer_controls[0],
			   ARRAY_SIZE(aic3x_right_pga_mixer_controls)),
	SND_SOC_DAPM_MUX("Right Line1R Mux", SND_SOC_NOPM, 0, 0,
			 &aic3x_right_line1_mux_controls),
	SND_SOC_DAPM_MUX("Right Line2R Mux", SND_SOC_NOPM, 0, 0,
			 &aic3x_right_line2_mux_controls),

	/* Mic Bias */
	SND_SOC_DAPM_MICBIAS("Mic Bias 2V", MICBIAS_CTRL, 6, 0),
	SND_SOC_DAPM_MICBIAS("Mic Bias 2.5V", MICBIAS_CTRL, 7, 0),
	SND_SOC_DAPM_MICBIAS("Mic Bias AVDD", MICBIAS_CTRL, 6, 0),
	SND_SOC_DAPM_MICBIAS("Mic Bias AVDD", MICBIAS_CTRL, 7, 0),

	/* Left PGA to Left Output bypass */
	SND_SOC_DAPM_MIXER("Left PGA Bypass Mixer", SND_SOC_NOPM, 0, 0,
			   &aic3x_left_pga_bp_mixer_controls[0],
			   ARRAY_SIZE(aic3x_left_pga_bp_mixer_controls)),

	/* Right PGA to Right Output bypass */
	SND_SOC_DAPM_MIXER("Right PGA Bypass Mixer", SND_SOC_NOPM, 0, 0,
			   &aic3x_right_pga_bp_mixer_controls[0],
			   ARRAY_SIZE(aic3x_right_pga_bp_mixer_controls)),

	/* Left Line2 to Left Output bypass */
	SND_SOC_DAPM_MIXER("Left Line2 Bypass Mixer", SND_SOC_NOPM, 0, 0,
			   &aic3x_left_line2_bp_mixer_controls[0],
			   ARRAY_SIZE(aic3x_left_line2_bp_mixer_controls)),

	/* Right Line2 to Right Output bypass */
	SND_SOC_DAPM_MIXER("Right Line2 Bypass Mixer", SND_SOC_NOPM, 0, 0,
			   &aic3x_right_line2_bp_mixer_controls[0],
			   ARRAY_SIZE(aic3x_right_line2_bp_mixer_controls)),

	SND_SOC_DAPM_OUTPUT("LLOUT"),
	SND_SOC_DAPM_OUTPUT("RLOUT"),
	SND_SOC_DAPM_OUTPUT("MONO_LOUT"),
	SND_SOC_DAPM_OUTPUT("HPLOUT"),
	SND_SOC_DAPM_OUTPUT("HPROUT"),
	SND_SOC_DAPM_OUTPUT("HPLCOM"),
	SND_SOC_DAPM_OUTPUT("HPRCOM"),

	SND_SOC_DAPM_INPUT("MIC3L"),
	SND_SOC_DAPM_INPUT("MIC3R"),
	SND_SOC_DAPM_INPUT("LINE1L"),
	SND_SOC_DAPM_INPUT("LINE1R"),
	SND_SOC_DAPM_INPUT("LINE2L"),
	SND_SOC_DAPM_INPUT("LINE2R"),
};

static const char *intercon[][3] = {
	/* Left Output */
	{"Left DAC Mux", "DAC_L1", "Left DAC"},
	{"Left DAC Mux", "DAC_L2", "Left DAC"},
	{"Left DAC Mux", "DAC_L3", "Left DAC"},

	{"Left DAC_L1 Mixer", "Line Switch", "Left DAC Mux"},
	{"Left DAC_L1 Mixer", "Mono Switch", "Left DAC Mux"},
	{"Left DAC_L1 Mixer", "HP Switch", "Left DAC Mux"},
	{"Left DAC_L1 Mixer", "HPCOM Switch", "Left DAC Mux"},
	{"Left Line Out", NULL, "Left DAC Mux"},
	{"Left HP Out", NULL, "Left DAC Mux"},

	{"Left HPCOM Mux", "differential of HPLOUT", "Left DAC_L1 Mixer"},
	{"Left HPCOM Mux", "constant VCM", "Left DAC_L1 Mixer"},
	{"Left HPCOM Mux", "single-ended", "Left DAC_L1 Mixer"},

	{"Left Line Out", NULL, "Left DAC_L1 Mixer"},
	{"Mono Out", NULL, "Left DAC_L1 Mixer"},
	{"Left HP Out", NULL, "Left DAC_L1 Mixer"},
	{"Left HP Com", NULL, "Left HPCOM Mux"},

	{"LLOUT", NULL, "Left Line Out"},
	{"LLOUT", NULL, "Left Line Out"},
	{"HPLOUT", NULL, "Left HP Out"},
	{"HPLCOM", NULL, "Left HP Com"},

	/* Right Output */
	{"Right DAC Mux", "DAC_R1", "Right DAC"},
	{"Right DAC Mux", "DAC_R2", "Right DAC"},
	{"Right DAC Mux", "DAC_R3", "Right DAC"},

	{"Right DAC_R1 Mixer", "Line Switch", "Right DAC Mux"},
	{"Right DAC_R1 Mixer", "Mono Switch", "Right DAC Mux"},
	{"Right DAC_R1 Mixer", "HP Switch", "Right DAC Mux"},
	{"Right DAC_R1 Mixer", "HPCOM Switch", "Right DAC Mux"},
	{"Right Line Out", NULL, "Right DAC Mux"},
	{"Right HP Out", NULL, "Right DAC Mux"},

	{"Right HPCOM Mux", "differential of HPROUT", "Right DAC_R1 Mixer"},
	{"Right HPCOM Mux", "constant VCM", "Right DAC_R1 Mixer"},
	{"Right HPCOM Mux", "single-ended", "Right DAC_R1 Mixer"},
	{"Right HPCOM Mux", "differential of HPLCOM", "Right DAC_R1 Mixer"},
	{"Right HPCOM Mux", "external feedback", "Right DAC_R1 Mixer"},

	{"Right Line Out", NULL, "Right DAC_R1 Mixer"},
	{"Mono Out", NULL, "Right DAC_R1 Mixer"},
	{"Right HP Out", NULL, "Right DAC_R1 Mixer"},
	{"Right HP Com", NULL, "Right HPCOM Mux"},

	{"RLOUT", NULL, "Right Line Out"},
	{"RLOUT", NULL, "Right Line Out"},
	{"HPROUT", NULL, "Right HP Out"},
	{"HPRCOM", NULL, "Right HP Com"},

	/* Mono Output */
	{"MONO_LOUT", NULL, "Mono Out"},
	{"MONO_LOUT", NULL, "Mono Out"},

	/* Left Input */
	{"Left Line1L Mux", "single-ended", "LINE1L"},
	{"Left Line1L Mux", "differential", "LINE1L"},

	{"Left Line2L Mux", "single-ended", "LINE2L"},
	{"Left Line2L Mux", "differential", "LINE2L"},

	{"Left PGA Mixer", "Line1L Switch", "Left Line1L Mux"},
	{"Left PGA Mixer", "Line2L Switch", "Left Line2L Mux"},
	{"Left PGA Mixer", "Mic3L Switch", "MIC3L"},

	{"Left ADC", NULL, "Left PGA Mixer"},

	/* Right Input */
	{"Right Line1R Mux", "single-ended", "LINE1R"},
	{"Right Line1R Mux", "differential", "LINE1R"},

	{"Right Line2R Mux", "single-ended", "LINE2R"},
	{"Right Line2R Mux", "differential", "LINE2R"},

	{"Right PGA Mixer", "Line1R Switch", "Right Line1R Mux"},
	{"Right PGA Mixer", "Line2R Switch", "Right Line2R Mux"},
	{"Right PGA Mixer", "Mic3R Switch", "MIC3R"},

	{"Right ADC", NULL, "Right PGA Mixer"},

	/* Left PGA Bypass */
	{"Left PGA Bypass Mixer", "Line Switch", "Left PGA Mixer"},
	{"Left PGA Bypass Mixer", "Mono Switch", "Left PGA Mixer"},
	{"Left PGA Bypass Mixer", "HP Switch", "Left PGA Mixer"},
	{"Left PGA Bypass Mixer", "HPCOM Switch", "Left PGA Mixer"},

	{"Left HPCOM Mux", "differential of HPLOUT", "Left PGA Bypass Mixer"},
	{"Left HPCOM Mux", "constant VCM", "Left PGA Bypass Mixer"},
	{"Left HPCOM Mux", "single-ended", "Left PGA Bypass Mixer"},

	{"Left Line Out", NULL, "Left PGA Bypass Mixer"},
	{"Mono Out", NULL, "Left PGA Bypass Mixer"},
	{"Left HP Out", NULL, "Left PGA Bypass Mixer"},

	/* Right PGA Bypass */
	{"Right PGA Bypass Mixer", "Line Switch", "Right PGA Mixer"},
	{"Right PGA Bypass Mixer", "Mono Switch", "Right PGA Mixer"},
	{"Right PGA Bypass Mixer", "HP Switch", "Right PGA Mixer"},
	{"Right PGA Bypass Mixer", "HPCOM Switch", "Right PGA Mixer"},

	{"Right HPCOM Mux", "differential of HPROUT", "Right PGA Bypass Mixer"},
	{"Right HPCOM Mux", "constant VCM", "Right PGA Bypass Mixer"},
	{"Right HPCOM Mux", "single-ended", "Right PGA Bypass Mixer"},
	{"Right HPCOM Mux", "differential of HPLCOM", "Right PGA Bypass Mixer"},
	{"Right HPCOM Mux", "external feedback", "Right PGA Bypass Mixer"},

	{"Right Line Out", NULL, "Right PGA Bypass Mixer"},
	{"Mono Out", NULL, "Right PGA Bypass Mixer"},
	{"Right HP Out", NULL, "Right PGA Bypass Mixer"},

	/* Left Line2 Bypass */
	{"Left Line2 Bypass Mixer", "Line Switch", "Left Line2L Mux"},
	{"Left Line2 Bypass Mixer", "Mono Switch", "Left Line2L Mux"},
	{"Left Line2 Bypass Mixer", "HP Switch", "Left Line2L Mux"},
	{"Left Line2 Bypass Mixer", "HPCOM Switch", "Left Line2L Mux"},

	{"Left HPCOM Mux", "differential of HPLOUT", "Left Line2 Bypass Mixer"},
	{"Left HPCOM Mux", "constant VCM", "Left Line2 Bypass Mixer"},
	{"Left HPCOM Mux", "single-ended", "Left Line2 Bypass Mixer"},

	{"Left Line Out", NULL, "Left Line2 Bypass Mixer"},
	{"Mono Out", NULL, "Left Line2 Bypass Mixer"},
	{"Left HP Out", NULL, "Left Line2 Bypass Mixer"},

	/* Right Line2 Bypass */
	{"Right Line2 Bypass Mixer", "Line Switch", "Right Line2R Mux"},
	{"Right Line2 Bypass Mixer", "Mono Switch", "Right Line2R Mux"},
	{"Right Line2 Bypass Mixer", "HP Switch", "Right Line2R Mux"},
	{"Right Line2 Bypass Mixer", "HPCOM Switch", "Right Line2R Mux"},

	{"Right HPCOM Mux", "differential of HPROUT", "Right Line2 Bypass Mixer"},
	{"Right HPCOM Mux", "constant VCM", "Right Line2 Bypass Mixer"},
	{"Right HPCOM Mux", "single-ended", "Right Line2 Bypass Mixer"},
	{"Right HPCOM Mux", "differential of HPLCOM", "Right Line2 Bypass Mixer"},
	{"Right HPCOM Mux", "external feedback", "Right Line2 Bypass Mixer"},

	{"Right Line Out", NULL, "Right Line2 Bypass Mixer"},
	{"Mono Out", NULL, "Right Line2 Bypass Mixer"},
	{"Right HP Out", NULL, "Right Line2 Bypass Mixer"},

	/* terminator */
	{NULL, NULL, NULL},
};

static int aic3x_add_widgets(struct snd_soc_codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic3x_dapm_widgets); i++)
		snd_soc_dapm_new_control(codec, &aic3x_dapm_widgets[i]);

	/* set up audio path interconnects */
	for (i = 0; intercon[i][0] != NULL; i++)
		snd_soc_dapm_connect_input(codec, intercon[i][0],
					   intercon[i][1], intercon[i][2]);

	snd_soc_dapm_new_widgets(codec);
	return 0;
}

static int aic3x_hw_params(struct snd_pcm_substream *substream,
			   struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->codec;
	struct aic3x_priv *aic3x = codec->private_data;
	int codec_clk = 0, bypass_pll = 0, fsref, last_clk = 0;
	u8 data, r, p, pll_q, pll_p = 1, pll_r = 1, pll_j = 1;
	u16 pll_d = 1;

	/* select data word length */
	data =
	    aic3x_read_reg_cache(codec, AIC3X_ASD_INTF_CTRLB) & (~(0x3 << 4));
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		data |= (0x01 << 4);
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		data |= (0x02 << 4);
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		data |= (0x03 << 4);
		break;
	}
	aic3x_write(codec, AIC3X_ASD_INTF_CTRLB, data);

	/* Fsref can be 44100 or 48000 */
	fsref = (params_rate(params) % 11025 == 0) ? 44100 : 48000;

	/* Try to find a value for Q which allows us to bypass the PLL and
	 * generate CODEC_CLK directly. */
	for (pll_q = 2; pll_q < 18; pll_q++)
		if (aic3x->sysclk / (128 * pll_q) == fsref) {
			bypass_pll = 1;
			break;
		}

	if (bypass_pll) {
		pll_q &= 0xf;
		aic3x_write(codec, AIC3X_PLL_PROGA_REG, pll_q << PLLQ_SHIFT);
		aic3x_write(codec, AIC3X_GPIOB_REG, CODEC_CLKIN_CLKDIV);
	} else
		aic3x_write(codec, AIC3X_GPIOB_REG, CODEC_CLKIN_PLLDIV);

	/* Route Left DAC to left channel input and
	 * right DAC to right channel input */
	data = (LDAC2LCH | RDAC2RCH);
	data |= (fsref == 44100) ? FSREF_44100 : FSREF_48000;
	if (params_rate(params) >= 64000)
		data |= DUAL_RATE_MODE;
	aic3x_write(codec, AIC3X_CODEC_DATAPATH_REG, data);

	/* codec sample rate select */
	data = (fsref * 20) / params_rate(params);
	if (params_rate(params) < 64000)
		data /= 2;
	data /= 5;
	data -= 2;
	data |= (data << 4);
	aic3x_write(codec, AIC3X_SAMPLE_RATE_SEL_REG, data);

	if (bypass_pll)
		return 0;

	/* Use PLL
	 * find an apropriate setup for j, d, r and p by iterating over
	 * p and r - j and d are calculated for each fraction.
	 * Up to 128 values are probed, the closest one wins the game.
	 * The sysclk is divided by 1000 to prevent integer overflows.
	 */
	codec_clk = (2048 * fsref) / (aic3x->sysclk / 1000);

	for (r = 1; r <= 16; r++)
		for (p = 1; p <= 8; p++) {
			int clk, tmp = (codec_clk * pll_r * 10) / pll_p;
			u8 j = tmp / 10000;
			u16 d = tmp % 10000;

			if (j > 63)
				continue;

			if (d != 0 && aic3x->sysclk < 10000000)
				continue;

			/* This is actually 1000 * ((j + (d/10000)) * r) / p
			 * The term had to be converted to get rid of the
			 * division by 10000 */
			clk = ((10000 * j * r) + (d * r)) / (10 * p);

			/* check whether this values get closer than the best
			 * ones we had before */
			if (abs(codec_clk - clk) < abs(codec_clk - last_clk)) {
				pll_j = j; pll_d = d; pll_r = r; pll_p = p;
				last_clk = clk;
			}

			/* Early exit for exact matches */
			if (clk == codec_clk)
				break;
		}

	if (last_clk == 0) {
		printk(KERN_ERR "%s(): unable to setup PLL\n", __func__);
		return -EINVAL;
	}

	data = aic3x_read_reg_cache(codec, AIC3X_PLL_PROGA_REG);
	aic3x_write(codec, AIC3X_PLL_PROGA_REG, data | (pll_p << PLLP_SHIFT));
	aic3x_write(codec, AIC3X_OVRF_STATUS_AND_PLLR_REG, pll_r << PLLR_SHIFT);
	aic3x_write(codec, AIC3X_PLL_PROGB_REG, pll_j << PLLJ_SHIFT);
	aic3x_write(codec, AIC3X_PLL_PROGC_REG, (pll_d >> 6) << PLLD_MSB_SHIFT);
	aic3x_write(codec, AIC3X_PLL_PROGD_REG,
		    (pll_d & 0x3F) << PLLD_LSB_SHIFT);

	return 0;
}

static int aic3x_mute(struct snd_soc_codec_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u8 ldac_reg = aic3x_read_reg_cache(codec, LDAC_VOL) & ~MUTE_ON;
	u8 rdac_reg = aic3x_read_reg_cache(codec, RDAC_VOL) & ~MUTE_ON;

	if (mute) {
		aic3x_write(codec, LDAC_VOL, ldac_reg | MUTE_ON);
		aic3x_write(codec, RDAC_VOL, rdac_reg | MUTE_ON);
	} else {
		aic3x_write(codec, LDAC_VOL, ldac_reg);
		aic3x_write(codec, RDAC_VOL, rdac_reg);
	}

	return 0;
}

static int aic3x_set_dai_sysclk(struct snd_soc_codec_dai *codec_dai,
				int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic3x_priv *aic3x = codec->private_data;

	aic3x->sysclk = freq;
	return 0;
}

static int aic3x_set_dai_fmt(struct snd_soc_codec_dai *codec_dai,
			     unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct aic3x_priv *aic3x = codec->private_data;
	u8 iface_areg = 0;
	u8 iface_breg = 0;

	/* set master/slave audio interface */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		aic3x->master = 1;
		iface_areg |= BIT_CLK_MASTER | WORD_CLK_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		aic3x->master = 0;
		break;
	default:
		return -EINVAL;
	}

	/* interface format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		break;
	case SND_SOC_DAIFMT_DSP_A:
		iface_breg |= (0x01 << 6);
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		iface_breg |= (0x02 << 6);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		iface_breg |= (0x03 << 6);
		break;
	default:
		return -EINVAL;
	}

	/* set iface */
	aic3x_write(codec, AIC3X_ASD_INTF_CTRLA, iface_areg);
	aic3x_write(codec, AIC3X_ASD_INTF_CTRLB, iface_breg);

	return 0;
}

static int aic3x_dapm_event(struct snd_soc_codec *codec, int event)
{
	struct aic3x_priv *aic3x = codec->private_data;
	u8 reg;

	switch (event) {
	case SNDRV_CTL_POWER_D0:
		/* all power is driven by DAPM system */
		if (aic3x->master) {
			/* enable pll */
			reg = aic3x_read_reg_cache(codec, AIC3X_PLL_PROGA_REG);
			aic3x_write(codec, AIC3X_PLL_PROGA_REG,
				    reg | PLL_ENABLE);
		}
		break;
	case SNDRV_CTL_POWER_D1:
	case SNDRV_CTL_POWER_D2:
		break;
	case SNDRV_CTL_POWER_D3hot:
		/*
		 * all power is driven by DAPM system,
		 * so output power is safe if bypass was set
		 */
		if (aic3x->master) {
			/* disable pll */
			reg = aic3x_read_reg_cache(codec, AIC3X_PLL_PROGA_REG);
			aic3x_write(codec, AIC3X_PLL_PROGA_REG,
				    reg & ~PLL_ENABLE);
		}
		break;
	case SNDRV_CTL_POWER_D3cold:
		/* force all power off */
		reg = aic3x_read_reg_cache(codec, LINE1L_2_LADC_CTRL);
		aic3x_write(codec, LINE1L_2_LADC_CTRL, reg & ~LADC_PWR_ON);
		reg = aic3x_read_reg_cache(codec, LINE1R_2_RADC_CTRL);
		aic3x_write(codec, LINE1R_2_RADC_CTRL, reg & ~RADC_PWR_ON);

		reg = aic3x_read_reg_cache(codec, DAC_PWR);
		aic3x_write(codec, DAC_PWR, reg & ~(LDAC_PWR_ON | RDAC_PWR_ON));

		reg = aic3x_read_reg_cache(codec, HPLOUT_CTRL);
		aic3x_write(codec, HPLOUT_CTRL, reg & ~HPLOUT_PWR_ON);
		reg = aic3x_read_reg_cache(codec, HPROUT_CTRL);
		aic3x_write(codec, HPROUT_CTRL, reg & ~HPROUT_PWR_ON);

		reg = aic3x_read_reg_cache(codec, HPLCOM_CTRL);
		aic3x_write(codec, HPLCOM_CTRL, reg & ~HPLCOM_PWR_ON);
		reg = aic3x_read_reg_cache(codec, HPRCOM_CTRL);
		aic3x_write(codec, HPRCOM_CTRL, reg & ~HPRCOM_PWR_ON);

		reg = aic3x_read_reg_cache(codec, MONOLOPM_CTRL);
		aic3x_write(codec, MONOLOPM_CTRL, reg & ~MONOLOPM_PWR_ON);

		reg = aic3x_read_reg_cache(codec, LLOPM_CTRL);
		aic3x_write(codec, LLOPM_CTRL, reg & ~LLOPM_PWR_ON);
		reg = aic3x_read_reg_cache(codec, RLOPM_CTRL);
		aic3x_write(codec, RLOPM_CTRL, reg & ~RLOPM_PWR_ON);

		if (aic3x->master) {
			/* disable pll */
			reg = aic3x_read_reg_cache(codec, AIC3X_PLL_PROGA_REG);
			aic3x_write(codec, AIC3X_PLL_PROGA_REG,
				    reg & ~PLL_ENABLE);
		}
		break;
	}
	codec->dapm_state = event;

	return 0;
}

void aic3x_set_gpio(struct snd_soc_codec *codec, int gpio, int state)
{
	u8 reg = gpio ? AIC3X_GPIO2_REG : AIC3X_GPIO1_REG;
	u8 bit = gpio ? 3: 0;
	u8 val = aic3x_read_reg_cache(codec, reg) & ~(1 << bit);
	aic3x_write(codec, reg, val | (!!state << bit));
}
EXPORT_SYMBOL_GPL(aic3x_set_gpio);

int aic3x_get_gpio(struct snd_soc_codec *codec, int gpio)
{
	u8 reg = gpio ? AIC3X_GPIO2_REG : AIC3X_GPIO1_REG;
	u8 val, bit = gpio ? 2: 1;

	aic3x_read(codec, reg, &val);
	return (val >> bit) & 1;
}
EXPORT_SYMBOL_GPL(aic3x_get_gpio);

int aic3x_headset_detected(struct snd_soc_codec *codec)
{
	u8 val;
	aic3x_read(codec, AIC3X_RT_IRQ_FLAGS_REG, &val);
	return (val >> 2) & 1;
}
EXPORT_SYMBOL_GPL(aic3x_headset_detected);

#define AIC3X_RATES	SNDRV_PCM_RATE_8000_96000
#define AIC3X_FORMATS	(SNDRV_PCM_FMTBIT_S16_LE | SNDRV_PCM_FMTBIT_S20_3LE | \
			 SNDRV_PCM_FMTBIT_S24_3LE | SNDRV_PCM_FMTBIT_S32_LE)

struct snd_soc_codec_dai aic3x_dai = {
	.name = "aic3x",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AIC3X_RATES,
		.formats = AIC3X_FORMATS,},
	.capture = {
		.stream_name = "Capture",
		.channels_min = 1,
		.channels_max = 2,
		.rates = AIC3X_RATES,
		.formats = AIC3X_FORMATS,},
	.ops = {
		.hw_params = aic3x_hw_params,
	},
	.dai_ops = {
		.digital_mute = aic3x_mute,
		.set_sysclk = aic3x_set_dai_sysclk,
		.set_fmt = aic3x_set_dai_fmt,
	}
};
EXPORT_SYMBOL_GPL(aic3x_dai);

static int aic3x_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	aic3x_dapm_event(codec, SNDRV_CTL_POWER_D3cold);

	return 0;
}

static int aic3x_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;
	int i;
	u8 data[2];
	u8 *cache = codec->reg_cache;

	/* Sync reg_cache with the hardware */
	for (i = 0; i < ARRAY_SIZE(aic3x_reg); i++) {
		data[0] = i;
		data[1] = cache[i];
		codec->hw_write(codec->control_data, data, 2);
	}

	aic3x_dapm_event(codec, codec->suspend_dapm_state);

	return 0;
}

/*
 * initialise the AIC3X driver
 * register the mixer and dsp interfaces with the kernel
 */
static int aic3x_init(struct snd_soc_device *socdev)
{
	struct snd_soc_codec *codec = socdev->codec;
	struct aic3x_setup_data *setup = socdev->codec_data;
	int reg, ret = 0;

	codec->name = "aic3x";
	codec->owner = THIS_MODULE;
	codec->read = aic3x_read_reg_cache;
	codec->write = aic3x_write;
	codec->dapm_event = aic3x_dapm_event;
	codec->dai = &aic3x_dai;
	codec->num_dai = 1;
	codec->reg_cache_size = sizeof(aic3x_reg);
	codec->reg_cache = kmemdup(aic3x_reg, sizeof(aic3x_reg), GFP_KERNEL);
	if (codec->reg_cache == NULL)
		return -ENOMEM;

	aic3x_write(codec, AIC3X_PAGE_SELECT, PAGE0_SELECT);
	aic3x_write(codec, AIC3X_RESET, SOFT_RESET);

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "aic3x: failed to create pcms\n");
		goto pcm_err;
	}

	/* DAC default volume and mute */
	aic3x_write(codec, LDAC_VOL, DEFAULT_VOL | MUTE_ON);
	aic3x_write(codec, RDAC_VOL, DEFAULT_VOL | MUTE_ON);

	/* DAC to HP default volume and route to Output mixer */
	aic3x_write(codec, DACL1_2_HPLOUT_VOL, DEFAULT_VOL | ROUTE_ON);
	aic3x_write(codec, DACR1_2_HPROUT_VOL, DEFAULT_VOL | ROUTE_ON);
	aic3x_write(codec, DACL1_2_HPLCOM_VOL, DEFAULT_VOL | ROUTE_ON);
	aic3x_write(codec, DACR1_2_HPRCOM_VOL, DEFAULT_VOL | ROUTE_ON);
	/* DAC to Line Out default volume and route to Output mixer */
	aic3x_write(codec, DACL1_2_LLOPM_VOL, DEFAULT_VOL | ROUTE_ON);
	aic3x_write(codec, DACR1_2_RLOPM_VOL, DEFAULT_VOL | ROUTE_ON);
	/* DAC to Mono Line Out default volume and route to Output mixer */
	aic3x_write(codec, DACL1_2_MONOLOPM_VOL, DEFAULT_VOL | ROUTE_ON);
	aic3x_write(codec, DACR1_2_MONOLOPM_VOL, DEFAULT_VOL | ROUTE_ON);

	/* unmute all outputs */
	reg = aic3x_read_reg_cache(codec, LLOPM_CTRL);
	aic3x_write(codec, LLOPM_CTRL, reg | UNMUTE);
	reg = aic3x_read_reg_cache(codec, RLOPM_CTRL);
	aic3x_write(codec, RLOPM_CTRL, reg | UNMUTE);
	reg = aic3x_read_reg_cache(codec, MONOLOPM_CTRL);
	aic3x_write(codec, MONOLOPM_CTRL, reg | UNMUTE);
	reg = aic3x_read_reg_cache(codec, HPLOUT_CTRL);
	aic3x_write(codec, HPLOUT_CTRL, reg | UNMUTE);
	reg = aic3x_read_reg_cache(codec, HPROUT_CTRL);
	aic3x_write(codec, HPROUT_CTRL, reg | UNMUTE);
	reg = aic3x_read_reg_cache(codec, HPLCOM_CTRL);
	aic3x_write(codec, HPLCOM_CTRL, reg | UNMUTE);
	reg = aic3x_read_reg_cache(codec, HPRCOM_CTRL);
	aic3x_write(codec, HPRCOM_CTRL, reg | UNMUTE);

	/* ADC default volume and unmute */
	aic3x_write(codec, LADC_VOL, DEFAULT_GAIN);
	aic3x_write(codec, RADC_VOL, DEFAULT_GAIN);
	/* By default route Line1 to ADC PGA mixer */
	aic3x_write(codec, LINE1L_2_LADC_CTRL, 0x0);
	aic3x_write(codec, LINE1R_2_RADC_CTRL, 0x0);

	/* PGA to HP Bypass default volume, disconnect from Output Mixer */
	aic3x_write(codec, PGAL_2_HPLOUT_VOL, DEFAULT_VOL);
	aic3x_write(codec, PGAR_2_HPROUT_VOL, DEFAULT_VOL);
	aic3x_write(codec, PGAL_2_HPLCOM_VOL, DEFAULT_VOL);
	aic3x_write(codec, PGAR_2_HPRCOM_VOL, DEFAULT_VOL);
	/* PGA to Line Out default volume, disconnect from Output Mixer */
	aic3x_write(codec, PGAL_2_LLOPM_VOL, DEFAULT_VOL);
	aic3x_write(codec, PGAR_2_RLOPM_VOL, DEFAULT_VOL);
	/* PGA to Mono Line Out default volume, disconnect from Output Mixer */
	aic3x_write(codec, PGAL_2_MONOLOPM_VOL, DEFAULT_VOL);
	aic3x_write(codec, PGAR_2_MONOLOPM_VOL, DEFAULT_VOL);

	/* Line2 to HP Bypass default volume, disconnect from Output Mixer */
	aic3x_write(codec, LINE2L_2_HPLOUT_VOL, DEFAULT_VOL);
	aic3x_write(codec, LINE2R_2_HPROUT_VOL, DEFAULT_VOL);
	aic3x_write(codec, LINE2L_2_HPLCOM_VOL, DEFAULT_VOL);
	aic3x_write(codec, LINE2R_2_HPRCOM_VOL, DEFAULT_VOL);
	/* Line2 Line Out default volume, disconnect from Output Mixer */
	aic3x_write(codec, LINE2L_2_LLOPM_VOL, DEFAULT_VOL);
	aic3x_write(codec, LINE2R_2_RLOPM_VOL, DEFAULT_VOL);
	/* Line2 to Mono Out default volume, disconnect from Output Mixer */
	aic3x_write(codec, LINE2L_2_MONOLOPM_VOL, DEFAULT_VOL);
	aic3x_write(codec, LINE2R_2_MONOLOPM_VOL, DEFAULT_VOL);

	/* off, with power on */
	aic3x_dapm_event(codec, SNDRV_CTL_POWER_D3hot);

	/* setup GPIO functions */
	aic3x_write(codec, AIC3X_GPIO1_REG, (setup->gpio_func[0] & 0xf) << 4);
	aic3x_write(codec, AIC3X_GPIO2_REG, (setup->gpio_func[1] & 0xf) << 4);

	aic3x_add_controls(codec);
	aic3x_add_widgets(codec);
	ret = snd_soc_register_card(socdev);
	if (ret < 0) {
		printk(KERN_ERR "aic3x: failed to register card\n");
		goto card_err;
	}

	return ret;

card_err:
	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
pcm_err:
	kfree(codec->reg_cache);
	return ret;
}

static struct snd_soc_device *aic3x_socdev;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
/*
 * AIC3X 2 wire address can be up to 4 devices with device addresses
 * 0x18, 0x19, 0x1A, 0x1B
 */
static unsigned short normal_i2c[] = { 0, I2C_CLIENT_END };

/* Magic definition of all other variables and things */
I2C_CLIENT_INSMOD;

static struct i2c_driver aic3x_i2c_driver;
static struct i2c_client client_template;

/*
 * If the i2c layer weren't so broken, we could pass this kind of data
 * around
 */
static int aic3x_codec_probe(struct i2c_adapter *adap, int addr, int kind)
{
	struct snd_soc_device *socdev = aic3x_socdev;
	struct aic3x_setup_data *setup = socdev->codec_data;
	struct snd_soc_codec *codec = socdev->codec;
	struct i2c_client *i2c;
	int ret;

	if (addr != setup->i2c_address)
		return -ENODEV;

	client_template.adapter = adap;
	client_template.addr = addr;

	i2c = kmemdup(&client_template, sizeof(client_template), GFP_KERNEL);
	if (i2c == NULL) {
		kfree(codec);
		return -ENOMEM;
	}
	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = i2c_attach_client(i2c);
	if (ret < 0) {
		printk(KERN_ERR "aic3x: failed to attach codec at addr %x\n",
		       addr);
		goto err;
	}

	ret = aic3x_init(socdev);
	if (ret < 0) {
		printk(KERN_ERR "aic3x: failed to initialise AIC3X\n");
		goto err;
	}
	return ret;

err:
	kfree(codec);
	kfree(i2c);
	return ret;
}

static int aic3x_i2c_detach(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	i2c_detach_client(client);
	kfree(codec->reg_cache);
	kfree(client);
	return 0;
}

static int aic3x_i2c_attach(struct i2c_adapter *adap)
{
	return i2c_probe(adap, &addr_data, aic3x_codec_probe);
}

/* machine i2c codec control layer */
static struct i2c_driver aic3x_i2c_driver = {
	.driver = {
		.name = "aic3x I2C Codec",
		.owner = THIS_MODULE,
	},
	.attach_adapter = aic3x_i2c_attach,
	.detach_client = aic3x_i2c_detach,
};

static struct i2c_client client_template = {
	.name = "AIC3X",
	.driver = &aic3x_i2c_driver,
};

static int aic3x_i2c_read(struct i2c_client *client, u8 *value, int len)
{
	value[0] = i2c_smbus_read_byte_data(client, value[0]);
	return (len == 1);
}
#endif

static int aic3x_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct aic3x_setup_data *setup;
	struct snd_soc_codec *codec;
	struct aic3x_priv *aic3x;
	int ret = 0;

	printk(KERN_INFO "AIC3X Audio Codec %s\n", AIC3X_VERSION);

	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	aic3x = kzalloc(sizeof(struct aic3x_priv), GFP_KERNEL);
	if (aic3x == NULL) {
		kfree(codec);
		return -ENOMEM;
	}

	codec->private_data = aic3x;
	socdev->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	aic3x_socdev = socdev;
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		normal_i2c[0] = setup->i2c_address;
		codec->hw_write = (hw_write_t) i2c_master_send;
		codec->hw_read = (hw_read_t) aic3x_i2c_read;
		ret = i2c_add_driver(&aic3x_i2c_driver);
		if (ret != 0)
			printk(KERN_ERR "can't add i2c driver");
	}
#else
	/* Add other interfaces here */
#endif
	return ret;
}

static int aic3x_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->codec;

	/* power down chip */
	if (codec->control_data)
		aic3x_dapm_event(codec, SNDRV_CTL_POWER_D3);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_del_driver(&aic3x_i2c_driver);
#endif
	kfree(codec->private_data);
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_aic3x = {
	.probe = aic3x_probe,
	.remove = aic3x_remove,
	.suspend = aic3x_suspend,
	.resume = aic3x_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_aic3x);

MODULE_DESCRIPTION("ASoC TLV320AIC3X codec driver");
MODULE_AUTHOR("Vladimir Barinov");
MODULE_LICENSE("GPL");
