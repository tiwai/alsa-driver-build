/*
 * mx27vis-aic32x4.c
 *
 * Copyright 2011 Vista Silicon S.L.
 *
 * Author: Javier Martin <javier.martin@vista-silicon.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/device.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <asm/mach-types.h>
#include <mach/iomux-mx27.h>

#include "../codecs/tlv320aic32x4.h"
#include "imx-ssi.h"
#include "imx-audmux.h"

#define MX27VIS_AMP_GAIN	0
#define MX27VIS_AMP_MUTE	1

#define MX27VIS_PIN_G0		(GPIO_PORTF + 9)
#define MX27VIS_PIN_G1		(GPIO_PORTF + 8)
#define MX27VIS_PIN_SDL		(GPIO_PORTE + 5)
#define MX27VIS_PIN_SDR		(GPIO_PORTF + 7)

static int mx27vis_amp_gain;
static int mx27vis_amp_mute;

static const int mx27vis_amp_pins[] = {
	MX27VIS_PIN_G0 | GPIO_GPIO | GPIO_OUT,
	MX27VIS_PIN_G1 | GPIO_GPIO | GPIO_OUT,
	MX27VIS_PIN_SDL | GPIO_GPIO | GPIO_OUT,
	MX27VIS_PIN_SDR | GPIO_GPIO | GPIO_OUT,
};

static int mx27vis_aic32x4_hw_params(struct snd_pcm_substream *substream,
			    struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	int ret;
	u32 dai_format;

	dai_format = SND_SOC_DAIFMT_DSP_B | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBM_CFM;

	/* set codec DAI configuration */
	snd_soc_dai_set_fmt(codec_dai, dai_format);

	/* set cpu DAI configuration */
	snd_soc_dai_set_fmt(cpu_dai, dai_format);

	ret = snd_soc_dai_set_sysclk(codec_dai, 0,
				     25000000, SND_SOC_CLOCK_OUT);
	if (ret) {
		pr_err("%s: failed setting codec sysclk\n", __func__);
		return ret;
	}

	ret = snd_soc_dai_set_sysclk(cpu_dai, IMX_SSP_SYS_CLK, 0,
				SND_SOC_CLOCK_IN);
	if (ret) {
		pr_err("can't set CPU system clock IMX_SSP_SYS_CLK\n");
		return ret;
	}

	return 0;
}

static struct snd_soc_ops mx27vis_aic32x4_snd_ops = {
	.hw_params	= mx27vis_aic32x4_hw_params,
};

static int mx27vis_amp_set(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	int value = ucontrol->value.integer.value[0];
	unsigned int reg = mc->reg;
	int max = mc->max;

	if (value > max)
		return -EINVAL;

	switch (reg) {
	case MX27VIS_AMP_GAIN:
		gpio_set_value(MX27VIS_PIN_G0, value & 1);
		gpio_set_value(MX27VIS_PIN_G1, value >> 1);
		mx27vis_amp_gain = value;
		break;
	case MX27VIS_AMP_MUTE:
		gpio_set_value(MX27VIS_PIN_SDL, value & 1);
		gpio_set_value(MX27VIS_PIN_SDR, value >> 1);
		mx27vis_amp_mute = value;
		break;
	}
	return 0;
}

static int mx27vis_amp_get(struct snd_kcontrol *kcontrol,
			    struct snd_ctl_elem_value *ucontrol)
{
	struct soc_mixer_control *mc =
		(struct soc_mixer_control *)kcontrol->private_value;
	unsigned int reg = mc->reg;

	switch (reg) {
	case MX27VIS_AMP_GAIN:
		ucontrol->value.integer.value[0] = mx27vis_amp_gain;
		break;
	case MX27VIS_AMP_MUTE:
		ucontrol->value.integer.value[0] = mx27vis_amp_mute;
		break;
	}
	return 0;
}

/* From 6dB to 24dB in steps of 6dB */
static const DECLARE_TLV_DB_SCALE(mx27vis_amp_tlv, 600, 600, 0);

static const struct snd_kcontrol_new mx27vis_aic32x4_controls[] = {
	SOC_DAPM_PIN_SWITCH("External Mic"),
	SOC_SINGLE_EXT_TLV("LO Ext Boost", MX27VIS_AMP_GAIN, 0, 3, 0,
		       mx27vis_amp_get, mx27vis_amp_set, mx27vis_amp_tlv),
	SOC_DOUBLE_EXT("LO Ext Mute Switch", MX27VIS_AMP_MUTE, 0, 1, 1, 0,
		       mx27vis_amp_get, mx27vis_amp_set),
};

static const struct snd_soc_dapm_widget aic32x4_dapm_widgets[] = {
	SND_SOC_DAPM_MIC("External Mic", NULL),
};

static const struct snd_soc_dapm_route aic32x4_dapm_routes[] = {
	{"Mic Bias", NULL, "External Mic"},
	{"IN1_R", NULL, "Mic Bias"},
	{"IN2_R", NULL, "Mic Bias"},
	{"IN3_R", NULL, "Mic Bias"},
	{"IN1_L", NULL, "Mic Bias"},
	{"IN2_L", NULL, "Mic Bias"},
	{"IN3_L", NULL, "Mic Bias"},
};

static struct snd_soc_dai_link mx27vis_aic32x4_dai = {
	.name		= "tlv320aic32x4",
	.stream_name	= "TLV320AIC32X4",
	.codec_dai_name	= "tlv320aic32x4-hifi",
	.platform_name	= "imx-pcm-audio.0",
	.codec_name	= "tlv320aic32x4.0-0018",
	.cpu_dai_name	= "imx-ssi.0",
	.ops		= &mx27vis_aic32x4_snd_ops,
};

static struct snd_soc_card mx27vis_aic32x4 = {
	.name		= "visstrim_m10-audio",
	.owner		= THIS_MODULE,
	.dai_link	= &mx27vis_aic32x4_dai,
	.num_links	= 1,
	.controls	= mx27vis_aic32x4_controls,
	.num_controls	= ARRAY_SIZE(mx27vis_aic32x4_controls),
	.dapm_widgets	= aic32x4_dapm_widgets,
	.num_dapm_widgets = ARRAY_SIZE(aic32x4_dapm_widgets),
	.dapm_routes	= aic32x4_dapm_routes,
	.num_dapm_routes = ARRAY_SIZE(aic32x4_dapm_routes),
};

static struct platform_device *mx27vis_aic32x4_snd_device;

static int __init mx27vis_aic32x4_init(void)
{
	int ret;

	mx27vis_aic32x4_snd_device = platform_device_alloc("soc-audio", -1);
	if (!mx27vis_aic32x4_snd_device)
		return -ENOMEM;

	platform_set_drvdata(mx27vis_aic32x4_snd_device, &mx27vis_aic32x4);
	ret = platform_device_add(mx27vis_aic32x4_snd_device);

	if (ret) {
		printk(KERN_ERR "ASoC: Platform device allocation failed\n");
		platform_device_put(mx27vis_aic32x4_snd_device);
	}

	/* Connect SSI0 as clock slave to SSI1 external pins */
	imx_audmux_v1_configure_port(MX27_AUDMUX_HPCR1_SSI0,
			IMX_AUDMUX_V1_PCR_SYN |
			IMX_AUDMUX_V1_PCR_TFSDIR |
			IMX_AUDMUX_V1_PCR_TCLKDIR |
			IMX_AUDMUX_V1_PCR_TFCSEL(MX27_AUDMUX_PPCR1_SSI_PINS_1) |
			IMX_AUDMUX_V1_PCR_RXDSEL(MX27_AUDMUX_PPCR1_SSI_PINS_1)
	);
	imx_audmux_v1_configure_port(MX27_AUDMUX_PPCR1_SSI_PINS_1,
			IMX_AUDMUX_V1_PCR_SYN |
			IMX_AUDMUX_V1_PCR_RXDSEL(MX27_AUDMUX_HPCR1_SSI0)
	);

	ret = mxc_gpio_setup_multiple_pins(mx27vis_amp_pins,
			ARRAY_SIZE(mx27vis_amp_pins), "MX27VIS_AMP");
	if (ret) {
		printk(KERN_ERR "ASoC: unable to setup gpios\n");
		platform_device_put(mx27vis_aic32x4_snd_device);
	}

	return ret;
}

static void __exit mx27vis_aic32x4_exit(void)
{
	platform_device_unregister(mx27vis_aic32x4_snd_device);
}

module_init(mx27vis_aic32x4_init);
module_exit(mx27vis_aic32x4_exit);

MODULE_AUTHOR("Javier Martin <javier.martin@vista-silicon.com>");
MODULE_DESCRIPTION("ALSA SoC AIC32X4 mx27 visstrim");
MODULE_LICENSE("GPL");
