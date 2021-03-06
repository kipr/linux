/*
 * linux/sound/soc/pxa/pxa3xx-ssp.c
 * Base on pxa2xx-ssp.c
 *
 * Copyright (C) 2007 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/initval.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>

#include <asm/io.h>
#include <asm/mach-types.h>

#include <plat/regs-ssp.h>
#include <plat/ssp.h>
#include <plat/dma.h>

#include "pxa3xx-pcm.h"
#include "pxa3xx-ssp.h"
#include "../codecs/autopower.h"
#include <linux/slab.h>

#define ARRAY_AND_SIZE(x)	x, ARRAY_SIZE(x)

/*
 * SSP audio private data
 */
struct ssp_priv {
	unsigned int sysclk;
	int dai_fmt;
};

static struct ssp_priv ssp_clk[4];
static struct ssp_dev ssp_dev[4];
#ifdef CONFIG_PM
static struct ssp_state ssp_state[4];
#endif

static char *ssp_names[4][2] = {
	{"SSP1 PCM out", "SSP1 PCM in",},
	{"SSP2 PCM out", "SSP2 PCM in",},
	{"SSP3 PCM out", "SSP3 PCM in",},
	{"SSP4 PCM out", "SSP4 PCM in",},
};

static int pxa3xx_ssp_startup(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct ssp_device *ssp;
	int ret = 0;
	u32 sscr0;

	if (!cpu_dai->active) {
		ret = ssp_init(&ssp_dev[cpu_dai->id], cpu_dai->id + 1,
				SSP_NO_IRQ);
		if(ret)
			return ret;
		ssp = ssp_dev[cpu_dai->id].ssp; 
		sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
		sscr0 &= ~SSCR0_SSE;
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);

		cpu_dai->private_data = ssp;
	}

	kfree(snd_soc_dai_get_dma_data(cpu_dai, substream));
	snd_soc_dai_set_dma_data(cpu_dai, substream, NULL);

	return ret;
}

static void pxa3xx_ssp_shutdown(struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct ssp_device *ssp = cpu_dai->private_data;
	u32 sscr0;

	if (!cpu_dai->active) {
		sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
		sscr0 &= ~SSCR0_SSE;
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);

		ssp_exit(ssp_dev+cpu_dai->id);
	}

	kfree(snd_soc_dai_get_dma_data(cpu_dai, substream));
	snd_soc_dai_set_dma_data(cpu_dai, substream, NULL);
}

#ifdef CONFIG_PM

static int pxa3xx_ssp_suspend(struct snd_soc_dai *cpu_dai)
{
	if (!cpu_dai->active)
		return 0;

	ssp_save_state(&ssp_dev[cpu_dai->id], &ssp_state[cpu_dai->id]);
	clk_disable(ssp_dev[cpu_dai->id].ssp->clk);
	return 0;
}

static int pxa3xx_ssp_resume(struct snd_soc_dai *cpu_dai)
{
	struct ssp_device *ssp = cpu_dai->private_data;
	u32 sscr0;

	if (!cpu_dai->active)
		return 0;

	clk_enable(ssp->clk);
	ssp_restore_state(&ssp_dev[cpu_dai->id], &ssp_state[cpu_dai->id]);

	sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
	sscr0 |= SSCR0_SSE;
	__raw_writel(sscr0, ssp->mmio_base + SSCR0);

	return 0;
}

#else
#define pxa3xx_ssp_suspend	NULL
#define pxa3xx_ssp_resume	NULL
#endif

/*
 * Set the SSP clock dividers.
 */
static int pxa3xx_ssp_set_dai_clkdiv(struct snd_soc_dai *cpu_dai,
	int div_id, int div)
{
	struct ssp_device *ssp = cpu_dai->private_data;
	u32 ssacd, sscr0;

	switch (div_id) {
	case PXA3XX_SSP_AUDIO_DIV_ACDS:
		ssacd = __raw_readl(ssp->mmio_base + SSACD);
		ssacd &= ~0x7;
		__raw_writel(ssacd, ssp->mmio_base + SSACD);
		ssacd |= SSACD_ACDS(div);
		__raw_writel(ssacd, ssp->mmio_base + SSACD);
		break;
	case PXA3XX_SSP_AUDIO_DIV_SCDB:
		ssacd = __raw_readl(ssp->mmio_base + SSACD);
		ssacd &= ~(SSACD_SCDB | SSACD_SCDX8);
		__raw_writel(ssacd, ssp->mmio_base + SSACD);
		switch (div) {
		case PXA3XX_SSP_CLK_SCDB_1:
			ssacd |= SSACD_SCDB;
			__raw_writel(ssacd, ssp->mmio_base + SSACD);
			break;
		case PXA3XX_SSP_CLK_SCDB_4:
			break;
		case PXA3XX_SSP_CLK_SCDB_8:
			ssacd |= SSACD_SCDX8;
			__raw_writel(ssacd, ssp->mmio_base + SSACD);
			break;
		default:
			return -EINVAL;
		}
		break;
	case PXA3XX_SSP_DIV_SCR:
		sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
		sscr0 &= ~SSCR0_SCR;
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);
		sscr0 |= SSCR0_SerClkDiv(div);
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);
		break;
	default:
		return -ENODEV;
	}

	return 0;
}

/*
 * Set up the SSP DAI format.
 * The SSP Port must be inactive before calling this function as the
 * physical interface format is changed.
 */
static int pxa3xx_ssp_set_dai_fmt(struct snd_soc_dai *cpu_dai,
		unsigned int fmt)
{
	struct ssp_device *ssp = cpu_dai->private_data;
	int dai_fmt = fmt & SND_SOC_DAIFMT_FORMAT_MASK;
	u32 sscr0, sscr1, sspsp;

	sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
	if (sscr0 & SSCR0_SSE)
		return 0;

	ssp_clk[cpu_dai->id].dai_fmt = dai_fmt;

	/*
	 * reset port settings
	 * PXA3xx docs say to use RxThresh = 8 and TxThresh = 7 with
	 * DMA bursts of 32
	 */
	__raw_writel(0, ssp->mmio_base + SSCR0);
	sscr1 = SSCR1_RxTresh(8) | SSCR1_TxTresh(7);
	__raw_writel(sscr1, ssp->mmio_base + SSCR1);
	__raw_writel(0, ssp->mmio_base + SSPSP);

	switch (dai_fmt) {
	case SND_SOC_DAIFMT_I2S:
		sscr0 = SSCR0_MOD | SSCR0_PSP;
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);
		sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
		sscr1 |= SSCR1_RWOT | SSCR1_TRAIL;
		__raw_writel(sscr1, ssp->mmio_base + SSCR1);

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			sspsp = __raw_readl(ssp->mmio_base + SSPSP);
			sspsp |= SSPSP_FSRT;
			__raw_writel(sspsp, ssp->mmio_base + SSPSP);
			break;
		case SND_SOC_DAIFMT_NB_IF:
			sspsp = __raw_readl(ssp->mmio_base + SSPSP);
			sspsp |= SSPSP_SFRMP | SSPSP_FSRT;
			__raw_writel(sspsp, ssp->mmio_base + SSPSP);
			break;
		case SND_SOC_DAIFMT_IB_IF:
			sspsp = __raw_readl(ssp->mmio_base + SSPSP);
			sspsp |= SSPSP_SFRMP;
			__raw_writel(sspsp, ssp->mmio_base + SSPSP);
			break;
		default:
			return -EINVAL;
		}
		break;
	case SND_SOC_DAIFMT_DSP_A:
	case SND_SOC_DAIFMT_DSP_B:
		sscr0 = SSCR0_MOD | SSCR0_PSP;
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);
		sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
		sscr1 |= SSCR1_TRAIL | SSCR1_RWOT;
		__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		if (dai_fmt == SND_SOC_DAIFMT_DSP_A)
			__raw_writel(SSPSP_FSRT, ssp->mmio_base + SSPSP);

		switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
		case SND_SOC_DAIFMT_NB_NF:
			sspsp = __raw_readl(ssp->mmio_base + SSPSP);
			sspsp |= SSPSP_SFRMP;
			__raw_writel(sspsp, ssp->mmio_base + SSPSP);
			break;
		case SND_SOC_DAIFMT_IB_IF:
			break;
		default:
			return -EINVAL;
		}

		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
		sscr1 |= SSCR1_SCLKDIR | SSCR1_SFRMDIR;
		__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		break;
	case SND_SOC_DAIFMT_CBM_CFS:
		sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
		sscr1 |= SSCR1_SCLKDIR;
		__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

/*
 * Set the SSP audio DMA parameters and sample size.
 * Can be called multiple times by oss emulation.
 */
static int pxa3xx_ssp_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params, struct snd_soc_dai * dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct ssp_device *ssp = cpu_dai->private_data;
	struct pxa3xx_pcm_dma_params *pcm;
	u32 sscr0, sspsp;
	u32 width;

	pcm = kmalloc(sizeof(struct pxa3xx_pcm_dma_params), GFP_KERNEL);
	if (pcm == NULL)
		return -ENOMEM;

	/* bit size */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
		sscr0 |= SSCR0_DataSize(16);
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);

		if ((sscr0 & SSCR0_FPCKE ) || (sscr0 & SSCR0_EDSS))
			width = DCMD_WIDTH4;
		else
			width = DCMD_WIDTH2;

		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
		sscr0 |= SSCR0_EDSS | SSCR0_DataSize(16);
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);

		width = DCMD_WIDTH4;

		break;
	default:
		return -EINVAL;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		pcm->name = ssp_names[cpu_dai->id][0];
		pcm->dcmd = DCMD_INCSRCADDR | DCMD_FLOWTRG;
		pcm->drcmr = &DRCMR(ssp->drcmr_tx);
		pcm->dev_addr	= ssp->phys_base + SSDR;
	} else {
		pcm->name = ssp_names[cpu_dai->id][1];
		pcm->dcmd = DCMD_INCTRGADDR | DCMD_FLOWSRC;
		pcm->drcmr = &DRCMR(ssp->drcmr_rx);
		pcm->dev_addr	= ssp->phys_base + SSDR;
	}

	pcm->dcmd |= DCMD_BURST16 | width;

	kfree(snd_soc_dai_get_dma_data(cpu_dai, substream));

	snd_soc_dai_set_dma_data(cpu_dai, substream, pcm);

	if (ssp_clk[cpu_dai->id].dai_fmt == SND_SOC_DAIFMT_I2S) {
		int sfrmwidth =
			snd_pcm_format_physical_width(params_format(params));
		sspsp = __raw_readl(ssp->mmio_base + SSPSP);
		sspsp |= SSPSP_SFRMWDTH(sfrmwidth);
		__raw_writel(sspsp, ssp->mmio_base + SSPSP);
	}

	return 0;
}

static int pxa3xx_ssp_trigger(struct snd_pcm_substream *substream, int cmd, struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct ssp_device *ssp = cpu_dai->private_data;
	int ret = 0;
	u32 sscr0, sscr1, sssr;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_RESUME:
		sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
		sscr0 |= SSCR0_SSE;
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
			sscr1 |= SSCR1_TSRE;
			__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		} else {
			sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
			sscr1 |= SSCR1_RSRE;
			__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		}
		sssr = __raw_readl(ssp->mmio_base + SSSR);
		__raw_writel(sssr, ssp->mmio_base + SSSR);
		break;
	case SNDRV_PCM_TRIGGER_START:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
			sscr1 |= SSCR1_TSRE;
			__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		} else {
			sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
			sscr1 |= SSCR1_RWOT;
			sscr1 |= SSCR1_RSRE;
			__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		}
		sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
		sscr0 |= SSCR0_SSE;
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
#warning	TODO autopower_disable()
#if 0
		if (machine_is_avengers_lite())
			autopower_disable();
#endif

		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
			sscr1 &= ~SSCR1_TSRE;
			__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		} else {
			sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
			sscr1 &= ~SSCR1_RSRE;
			__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		}
		break;
	case SNDRV_PCM_TRIGGER_SUSPEND:
		sscr0 = __raw_readl(ssp->mmio_base + SSCR0);
		sscr0 &= ~SSCR0_SSE;
		__raw_writel(sscr0, ssp->mmio_base + SSCR0);
		break;
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
			sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
			sscr1 &= ~SSCR1_TSRE;
			__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		} else {
			sscr1 = __raw_readl(ssp->mmio_base + SSCR1);
			sscr1 &= ~SSCR1_RSRE;
			__raw_writel(sscr1, ssp->mmio_base + SSCR1);
		}
		break;

	default:
		ret = -EINVAL;
	}

	return ret;
}

#define PXA3XX_SSP_RATES 0xffffffff
#define PXA3XX_SSP_FORMATS 0xffffffff

static struct snd_soc_dai_ops pxa3xx_ssp_dai_ops = {
		.startup	= pxa3xx_ssp_startup,
		.shutdown	= pxa3xx_ssp_shutdown,
		.trigger    = pxa3xx_ssp_trigger,
		.hw_params	= pxa3xx_ssp_hw_params,
		.set_fmt	= pxa3xx_ssp_set_dai_fmt,
		.set_clkdiv = pxa3xx_ssp_set_dai_clkdiv,
};

struct snd_soc_dai pxa3xx_ssp_dai[] = {
	{	.name = "pxa168-ssp",
		.id = 0,
		.suspend = pxa3xx_ssp_suspend,
		.resume = pxa3xx_ssp_resume,
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},
		.ops = &pxa3xx_ssp_dai_ops,
	},
	{	.name = "pxa168-ssp",
		.id = 1,
		.suspend = pxa3xx_ssp_suspend,
		.resume = pxa3xx_ssp_resume,
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},
			.ops = &pxa3xx_ssp_dai_ops,
	},
	{	.name = "pxa168-ssp",
		.id = 2,
		.suspend = pxa3xx_ssp_suspend,
		.resume = pxa3xx_ssp_resume,
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},
			.ops = &pxa3xx_ssp_dai_ops,
	},
	{	.name = "pxa168-ssp",
		.id = 3,
		.suspend = pxa3xx_ssp_suspend,
		.resume = pxa3xx_ssp_resume,
		.playback = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},
		.capture = {
			.channels_min = 1,
			.channels_max = 2,
			.rates = PXA3XX_SSP_RATES|SNDRV_PCM_RATE_32000,
			.formats = PXA3XX_SSP_FORMATS},
			.ops = &pxa3xx_ssp_dai_ops,
	},
	{	.name = "Grayback-ssp",
		.id = 4,
		.playback = {
			.channels_min = 1,
			.channels_max = 1,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},
		.capture = {
			.channels_min = 1,
			.channels_max = 1,
			.rates = PXA3XX_SSP_RATES,
			.formats = PXA3XX_SSP_FORMATS,},

	},
};
EXPORT_SYMBOL_GPL(pxa3xx_ssp_dai);

static int __init pxa3xx_ssp_modinit(void)
{
	return snd_soc_register_dais(ARRAY_AND_SIZE(pxa3xx_ssp_dai));
}
module_init(pxa3xx_ssp_modinit);

static void __exit pxa3xx_ssp_exit(void)
{
	snd_soc_unregister_dais(ARRAY_AND_SIZE(pxa3xx_ssp_dai));
}
module_exit(pxa3xx_ssp_exit);

/* Module information */
MODULE_AUTHOR("bin.yang@marvell.com");
MODULE_DESCRIPTION("pxa3xx SSP/PCM SoC Interface");
MODULE_LICENSE("GPL");
