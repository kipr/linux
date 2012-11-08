/*
 * es8328.c  --  ES8328 ALSA SoC Audio driver
 *
 * Copyright 2008 Wolfson Microelectronics plc
 *
 * Author: Mark Brown <broonie@opensource.wolfsonmicro.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/initval.h>
#include <sound/tlv.h>

#include "es8328.h"

#define KOVAN 1

struct snd_soc_codec_device soc_codec_dev_es8328;

/*
 * We can't read the ES8328 register space so we cache them instead.
 * Note that the defaults here aren't the physical defaults, we latch
 * the volume update bits, mute the output and enable infinite zero
 * detect.
 */
/*
static const u16 es8328_reg_defaults[] = {
	0x1ff,
	0x1ff,
	0x001,
	0x100,
};
*/

static const DECLARE_TLV_DB_SCALE(es8328_tlv, -12750, 50, 1);

static const struct snd_kcontrol_new es8328_snd_controls[] = {
	SOC_SINGLE("PCM Volume", ES8328_DACCONTROL26, 0, 36, 0),
};

/*
 * DAPM controls.
 */
static const struct snd_soc_dapm_widget es8328_dapm_widgets[] = {
};

static const struct snd_soc_dapm_route intercon[] = {
};

static int es8328_add_widgets(struct snd_soc_codec *codec)
{
	snd_soc_dapm_new_controls(codec, es8328_dapm_widgets,
				  ARRAY_SIZE(es8328_dapm_widgets));

	snd_soc_dapm_add_routes(codec, intercon, ARRAY_SIZE(intercon));

	return 0;
}

static int es8328_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	u16 mute_reg = snd_soc_read(codec, ES8328_DACCONTROL3);

	if (mute)
		snd_soc_write(codec, ES8328_DACCONTROL3, mute_reg | 4);
	else
		snd_soc_write(codec, ES8328_DACCONTROL3, mute_reg & ~4);

	return 0;
}


static int es8328_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_device *socdev = rtd->socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
/*
	u16 dac = snd_soc_read(codec, ES8328_DACCTL);

	dac &= ~0x18;

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		dac |= 0x10;
		break;
	case SNDRV_PCM_FORMAT_S24_LE:
		dac |= 0x08;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, ES8328_DACCTL, dac);
*/
	snd_soc_write(codec, ES8328_CHIPPOWER,  0xaa);

	return 0;
}

static int es8328_set_dai_fmt(struct snd_soc_dai *codec_dai,
		unsigned int fmt)
{
#if 0
	struct snd_soc_codec *codec = codec_dai->codec;
	u16 iface = snd_soc_read(codec, ES8328_IFCTL);

	/* Currently only I2S is supported by the driver, though the
	 * hardware is more flexible.
	 */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		iface |= 1;
		break;
	default:
		return -EINVAL;
	}

	/* The hardware only support full slave mode */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		iface &= ~0x22;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		iface |=  0x20;
		iface &= ~0x02;
		break;
	case SND_SOC_DAIFMT_NB_IF:
		iface |= 0x02;
		iface &= ~0x20;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		iface |= 0x22;
		break;
	default:
		return -EINVAL;
	}

	snd_soc_write(codec, ES8328_IFCTL, iface);
#endif
	return 0;
}

static int es8328_set_bias_level(struct snd_soc_codec *codec,
				 enum snd_soc_bias_level level)
{
	switch (level) {
	case SND_SOC_BIAS_ON:
	case SND_SOC_BIAS_PREPARE:
	case SND_SOC_BIAS_STANDBY:
		break;

	case SND_SOC_BIAS_OFF:
		break;
	}
	codec->bias_level = level;
	return 0;
}

#define ES8328_RATES (SNDRV_PCM_RATE_44100)

#define ES8328_FORMATS (SNDRV_PCM_FMTBIT_S16_LE)

static struct snd_soc_dai_ops es8328_dai_ops = {
	.hw_params	= es8328_hw_params,
	.digital_mute	= es8328_mute,
	.set_fmt	= es8328_set_dai_fmt,
};

struct snd_soc_dai es8328_dai = {
	.name = "ES8328",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 2,
		.rates = ES8328_RATES,
		.formats = ES8328_FORMATS,
	},
	.ops = &es8328_dai_ops,
};
EXPORT_SYMBOL_GPL(es8328_dai);

static int es8328_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	es8328_set_bias_level(codec, SND_SOC_BIAS_OFF);

	return 0;
}

static int es8328_resume(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	es8328_set_bias_level(codec, codec->suspend_bias_level);

	return 0;
}

/*
 * initialise the ES8328 driver
 * register the mixer and dsp interfaces with the kernel
 */
static int es8328_init(struct snd_soc_device *socdev,
		       enum snd_soc_control_type control)
{
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret = 0;

	codec->name = "ES8328";
	codec->owner = THIS_MODULE;
	codec->set_bias_level = es8328_set_bias_level;
	codec->dai = &es8328_dai;
	codec->num_dai = 1;
	codec->bias_level = SND_SOC_BIAS_OFF;
	codec->reg_cache_size = ES8328_REG_MAX;
	codec->reg_cache = kzalloc(ES8328_REG_MAX, 0);
	if (codec->reg_cache == NULL)
		return -ENOMEM;

	ret = snd_soc_codec_set_cache_io(codec, 8, 8, control);
	if (ret < 0) {
		printk(KERN_ERR "es8328: failed to configure cache I/O: %d\n",
		       ret);
		goto err;
	}

	/* register pcms */
	ret = snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (ret < 0) {
		printk(KERN_ERR "es8328: failed to create pcms\n");
		goto err;
	}

	/* power on device */
	es8328_set_bias_level(codec, SND_SOC_BIAS_STANDBY);

	snd_soc_add_controls(codec, es8328_snd_controls,
				ARRAY_SIZE(es8328_snd_controls));
	es8328_add_widgets(codec);

			snd_soc_write(codec, ES8328_MASTERMODE, 0xc0);
			snd_soc_write(codec, ES8328_CHIPPOWER, 0xf3);
			snd_soc_write(codec, ES8328_CONTROL1 , 0x05);

			snd_soc_write(codec, ES8328_CONTROL2, 0x40);

			/* Power down ADC */
			snd_soc_write(codec, ES8328_ADCPOWER, 0xfc);

			/* Power up LOUT2, and power down ROUT2 and xOUT1 */
			snd_soc_write(codec, ES8328_DACPOWER,  0x48);

			/* Set I2S to 16-bit mode */
			snd_soc_write(codec, ES8328_DACCONTROL1, 0x18);

			/* Frequency clock of 256 */
			snd_soc_write(codec, ES8328_DACCONTROL2, 0x02);

			/* No attenuation */
			snd_soc_write(codec, ES8328_DACCONTROL4, 0x00);
			snd_soc_write(codec, ES8328_DACCONTROL5, 0x00);

			/* Set LIN2 for the output mixer */
			snd_soc_write(codec, ES8328_DACCONTROL16, 0x09);



			/* Point only the left DAC at the left mixer */
			snd_soc_write(codec, ES8328_DACCONTROL17, 0x80);

			/* Disable all other outputs */
			snd_soc_write(codec, ES8328_DACCONTROL18, 0x00);
			snd_soc_write(codec, ES8328_DACCONTROL19, 0x00);
			snd_soc_write(codec, ES8328_DACCONTROL20, 0x00);



			/* Set mono mode for DACL, and mute DACR */
			snd_soc_write(codec, ES8328_DACCONTROL7, 0x66);

			/* Power on the chip */
			snd_soc_write(codec, ES8328_CHIPPOWER, 0x00);


			/* Turn off muting */
			snd_soc_write(codec, ES8328_DACCONTROL3, 0x30);
			

	return ret;

err:
	kfree(codec->reg_cache);
	return ret;
}

static struct snd_soc_device *es8328_socdev;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)

/*
 * ES8328 2 wire address is determined by GPIO5
 * state during powerup.
 *    low  = 0x1a
 *    high = 0x1b
 */

static int es8328_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct snd_soc_device *socdev = es8328_socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret;

	i2c_set_clientdata(i2c, codec);
	codec->control_data = i2c;

	ret = es8328_init(socdev, SND_SOC_I2C);
	if (ret < 0)
		pr_err("failed to initialise ES8328\n");

	return ret;
}

static int es8328_i2c_remove(struct i2c_client *client)
{
	struct snd_soc_codec *codec = i2c_get_clientdata(client);
	kfree(codec->reg_cache);
	return 0;
}

static const struct i2c_device_id es8328_i2c_id[] = {
	{ "es8328", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, es8328_i2c_id);

static struct i2c_driver es8328_i2c_driver = {
	.driver = {
		.name = "ES8328 I2C Codec",
		.owner = THIS_MODULE,
	},
	.probe =    es8328_i2c_probe,
	.remove =   es8328_i2c_remove,
	.id_table = es8328_i2c_id,
};

static int es8328_add_i2c_device(struct platform_device *pdev,
				 const struct es8328_setup_data *setup)
{
	int ret;

	ret = i2c_add_driver(&es8328_i2c_driver);
	if (ret != 0) {
		dev_err(&pdev->dev, "can't add i2c driver\n");
		return ret;
	}

	return 0;
}
#endif

#if defined(CONFIG_SPI_MASTER)
#ifndef KOVAN
static int __devinit es8328_spi_probe(struct spi_device *spi)
{
	struct snd_soc_device *socdev = es8328_socdev;
	struct snd_soc_codec *codec = socdev->card->codec;
	int ret;

	codec->control_data = spi;

	ret = es8328_init(socdev, SND_SOC_SPI);
	if (ret < 0)
		dev_err(&spi->dev, "failed to initialise ES8328\n");

	return ret;
}

static int __devexit es8328_spi_remove(struct spi_device *spi)
{
	return 0;
}

static struct spi_driver es8328_spi_driver = {
	.driver = {
		.name	= "es8328",
		.bus	= &spi_bus_type,
		.owner	= THIS_MODULE,
	},
	.probe		= es8328_spi_probe,
	.remove		= __devexit_p(es8328_spi_remove),
};
#endif /* ifndef KOVAN */
#endif /* CONFIG_SPI_MASTER */

static int es8328_probe(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct es8328_setup_data *setup;
	struct snd_soc_codec *codec;
	int ret = 0;

	printk(">>> Probing ES8328\n");
	setup = socdev->codec_data;
	codec = kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL);
	if (codec == NULL)
		return -ENOMEM;

	socdev->card->codec = codec;
	mutex_init(&codec->mutex);
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);

	es8328_socdev = socdev;
	ret = -ENODEV;

#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	if (setup->i2c_address) {
		ret = es8328_add_i2c_device(pdev, setup);
	}
#endif
#if defined(CONFIG_SPI_MASTER)
#ifndef KOVAN
	if (setup->spi) {
		ret = spi_register_driver(&es8328_spi_driver);
		if (ret != 0)
			printk(KERN_ERR "can't add spi driver");
	}
#endif
#endif
	if (ret != 0)
		kfree(codec);

	return ret;
}

/* power down chip */
static int es8328_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev = platform_get_drvdata(pdev);
	struct snd_soc_codec *codec = socdev->card->codec;

	if (codec->control_data)
		es8328_set_bias_level(codec, SND_SOC_BIAS_OFF);

	snd_soc_free_pcms(socdev);
	snd_soc_dapm_free(socdev);
#if defined(CONFIG_I2C) || defined(CONFIG_I2C_MODULE)
	i2c_unregister_device(codec->control_data);
	i2c_del_driver(&es8328_i2c_driver);
#endif
#if defined(CONFIG_SPI_MASTER)
#ifndef KOVAN
	spi_unregister_driver(&es8328_spi_driver);
#endif
#endif
	kfree(codec);

	return 0;
}

struct snd_soc_codec_device soc_codec_dev_es8328 = {
	.probe = 	es8328_probe,
	.remove = 	es8328_remove,
	.suspend = 	es8328_suspend,
	.resume =	es8328_resume,
};
EXPORT_SYMBOL_GPL(soc_codec_dev_es8328);

static int __init es8328_modinit(void)
{
	return snd_soc_register_dai(&es8328_dai);
}
module_init(es8328_modinit);

static void __exit es8328_exit(void)
{
	snd_soc_unregister_dai(&es8328_dai);
}
module_exit(es8328_exit);

MODULE_DESCRIPTION("ASoC ES8328 driver");
MODULE_AUTHOR("Mark Brown <broonie@opensource.wolfsonmicro.com>");
MODULE_LICENSE("GPL");
