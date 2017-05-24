/*
 * PCM1690 ASoC codec driver
 *
 * Author: Wiebe Cazemier wiebe@halfgaar.net
 *
 * Based on PCM1690 from alsa-soc, by StreamUnlimited GmbH,
 *   Marek Belisko <marek.belisko@streamunlimited.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

// TODO: this DAC also supports 32 bit

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/tlv.h>

#define PCM1690_PCM_FORMATS (SNDRV_PCM_FMTBIT_S16_LE  |		\
			     SNDRV_PCM_FMTBIT_S24_LE)

#define PCM1690_PCM_RATES   (SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000 | \
			     SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100  | \
			     SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_88200  | \
			     SNDRV_PCM_RATE_96000 | SNDRV_PCM_RATE_192000)

#define PCM1690_SOFT_MUTE_ALL		0xff
#define PCM1690_DEEMPH_RATE_MASK	0x30

#define PCM1690_ATT_CONTROL(X)	(71 + X) /* Attenuation level */
#define PCM1690_SOFT_MUTE	0x44	/* Soft mute control register */
#define PCM1690_FMT_CONTROL	0x41	/* Audio interface data format */
#define PCM1690_DEEMPH_CONTROL	0x46	/* De-emphasis control */
#define PCM1690_ZERO_DETECT_STATUS	0x45	/* Zero detect status reg */

static const struct reg_default pcm1690_reg_defaults[] = {
	{ 0x40,	0xC0 }, // 11000000, only the MRST and SRST reset registers are 1.
	{ 0x41,	0x00 },
	{ 0x42,	0x00 },
	{ 0x43,	0x00 },
	{ 0x44,	0x00 },
	{ 0x45,	0x00 },
	{ 0x46,	0x00 },
	{ 0x47,	0x00 },
	{ 0x48,	0xff }, // Default volume at 0 dB attenuation
	{ 0x49,	0xff }, // Default volume at 0 dB attenuation
	{ 0x4A,	0xff }, // Default volume at 0 dB attenuation
	{ 0x4B,	0xff }, // Default volume at 0 dB attenuation
	{ 0x4C,	0xff }, // Default volume at 0 dB attenuation
	{ 0x4D,	0xff }, // Default volume at 0 dB attenuation
	{ 0x4E,	0xff }, // Default volume at 0 dB attenuation
	{ 0x4F,	0xff }, // Default volume at 0 dB attenuation
};

static bool pcm1690_accessible_reg(struct device *dev, unsigned int reg)
{
	return reg >= 0x40 && reg <= 0x4f;
}

static bool pcm1690_writeable_reg(struct device *dev, unsigned register reg)
{
	return pcm1690_accessible_reg(dev, reg) &&
		(reg != PCM1690_ZERO_DETECT_STATUS);
}

struct pcm1690_private {
	struct regmap *regmap;
	unsigned int format;
	/* Current deemphasis status */
	unsigned int deemph;
	/* Current rate for deemphasis control */
	unsigned int rate;
	unsigned int tdm_slots;
};

static const int pcm1690_deemph[] = { 0, 48000, 44100, 32000 };

// Dummy function because I disabled it. See elsewhere.
static int pcm1690_set_deemph(struct snd_soc_codec *codec)
{
  return 0;
}

/*
static int pcm1690_set_deemph(struct snd_soc_codec *codec)
{
	struct pcm1690_private *priv = snd_soc_codec_get_drvdata(codec);
	int i = 0, val = -1;

  if (!priv->deemph)
  {
    return regmap_update_bits(priv->regmap, PCM1690_DEEMPH_CONTROL, PCM1690_DEEMPH_RATE_MASK, 0);
  }
  else
  {
    for (i = 0; i < ARRAY_SIZE(pcm1690_deemph); i++)
      if (pcm1690_deemph[i] == priv->rate)
        val = i;

    if (val != -1)
    {
      return regmap_update_bits(priv->regmap, PCM1690_DEEMPH_CONTROL, PCM1690_DEEMPH_RATE_MASK, val);
    }
  }

  return -1;
}

static int pcm1690_get_deemph(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct pcm1690_private *priv = snd_soc_codec_get_drvdata(codec);

	ucontrol->value.enumerated.item[0] = priv->deemph;

	return 0;
}

static int pcm1690_put_deemph(struct snd_kcontrol *kcontrol,
			      struct snd_ctl_elem_value *ucontrol)
{
	struct snd_soc_codec *codec = snd_kcontrol_chip(kcontrol);
	struct pcm1690_private *priv = snd_soc_codec_get_drvdata(codec);

	priv->deemph = ucontrol->value.enumerated.item[0];

	return pcm1690_set_deemph(codec);
}
*/

static int pcm1690_set_dai_fmt(struct snd_soc_dai *codec_dai,
			      unsigned int format)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct pcm1690_private *priv = snd_soc_codec_get_drvdata(codec);

	/* The PCM1690 can only be slave to all clocks */
	if ((format & SND_SOC_DAIFMT_MASTER_MASK) != SND_SOC_DAIFMT_CBS_CFS) {
		dev_err(codec->dev, "Invalid clocking mode. PCM1690 can only be slave: (SND_SOC_DAIFMT_CBS_CFS)\n");
		return -EINVAL;
	}

	priv->format = format;

	return 0;
}

static int pcm1690_digital_mute(struct snd_soc_dai *dai, int mute)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pcm1690_private *priv = snd_soc_codec_get_drvdata(codec);
	int val;

	if (mute)
		val = PCM1690_SOFT_MUTE_ALL;
	else
		val = 0;

	return regmap_write(priv->regmap, PCM1690_SOFT_MUTE, val);
}

static int pcm1690_hw_params(struct snd_pcm_substream *substream,
			     struct snd_pcm_hw_params *params,
			     struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pcm1690_private *priv = snd_soc_codec_get_drvdata(codec);
	int val = 0, ret;
	int pcm_format = params_format(params);
	unsigned int masked_format = priv->format & SND_SOC_DAIFMT_FORMAT_MASK ;

	priv->rate = params_rate(params);

  if (priv->tdm_slots == 8)
  {
    switch (masked_format) {
    case SND_SOC_DAIFMT_RIGHT_J:
      dev_err(codec->dev, "Invalid DAI format: with 8 TDM slots, you can't have SND_SOC_DAIFMT_RIGHT_J.\n");
      return -EINVAL;
    case SND_SOC_DAIFMT_I2S:
      val = 0x06;
      break;
    case SND_SOC_DAIFMT_LEFT_J:
      val = 0x07;
      break;
    default:
      dev_err(codec->dev, "Invalid DAI format\n");
      return -EINVAL;
    }
  }
  else if (priv->tdm_slots == 2)
  {
    switch (masked_format)
    {
      case SND_SOC_DAIFMT_RIGHT_J:
        if (pcm_format == SNDRV_PCM_FORMAT_S24_LE)
          val = 0x02;
        else if (pcm_format == SNDRV_PCM_FORMAT_S16_LE)
          val = 0x03;
        break;
      case SND_SOC_DAIFMT_I2S:
        val = 0x00;
        break;
      case SND_SOC_DAIFMT_LEFT_J:
        val = 0x01;
        break;
      default:
        dev_err(codec->dev, "Invalid DAI format\n");
        return -EINVAL;
    }
  }
  else
  {
    dev_err(codec->dev, "tdm_slots must be 2 or 8\n");
    return -EINVAL;
  }

	ret = regmap_update_bits(priv->regmap, PCM1690_FMT_CONTROL, 0x0f, val);
	if (ret < 0)
		return ret;

	return pcm1690_set_deemph(codec);
}

static int pcm1690_set_tdm_slots(struct snd_soc_dai *dai, unsigned int tx_mask, unsigned int rx_mask, int slots, int slot_width)
{
	struct snd_soc_codec *codec = dai->codec;
	struct pcm1690_private *priv = snd_soc_codec_get_drvdata(codec);

	priv->tdm_slots = slots;
	return 0;
}

static const struct snd_soc_dai_ops pcm1690_dai_ops = {
	.set_fmt	= pcm1690_set_dai_fmt,
	.hw_params	= pcm1690_hw_params,
	.digital_mute	= pcm1690_digital_mute,
	.set_tdm_slot	= pcm1690_set_tdm_slots,
};

static const struct snd_soc_dapm_widget pcm1690_dapm_widgets[] = {
SND_SOC_DAPM_OUTPUT("VOUT1"),
SND_SOC_DAPM_OUTPUT("VOUT2"),
SND_SOC_DAPM_OUTPUT("VOUT3"),
SND_SOC_DAPM_OUTPUT("VOUT4"),
SND_SOC_DAPM_OUTPUT("VOUT5"),
SND_SOC_DAPM_OUTPUT("VOUT6"),
SND_SOC_DAPM_OUTPUT("VOUT7"),
SND_SOC_DAPM_OUTPUT("VOUT8"),
};

static const struct snd_soc_dapm_route pcm1690_dapm_routes[] = {
	{ "VOUT1", NULL, "Playback" },
	{ "VOUT2", NULL, "Playback" },
	{ "VOUT3", NULL, "Playback" },
	{ "VOUT4", NULL, "Playback" },
	{ "VOUT5", NULL, "Playback" },
	{ "VOUT6", NULL, "Playback" },
	{ "VOUT7", NULL, "Playback" },
	{ "VOUT8", NULL, "Playback" },
};

static const DECLARE_TLV_DB_SCALE(pcm1690_dac_tlv, -6350, 50, 1);

static const struct snd_kcontrol_new pcm1690_controls[] = {
	SOC_DOUBLE_R_TLV("Channel 1/2 Playback Volume",
			PCM1690_ATT_CONTROL(1), PCM1690_ATT_CONTROL(2), 0,
			0x7f, 0, pcm1690_dac_tlv),
	SOC_DOUBLE_R_TLV("Channel 3/4 Playback Volume",
			PCM1690_ATT_CONTROL(3), PCM1690_ATT_CONTROL(4), 0,
			0x7f, 0, pcm1690_dac_tlv),
	SOC_DOUBLE_R_TLV("Channel 5/6 Playback Volume",
			PCM1690_ATT_CONTROL(5), PCM1690_ATT_CONTROL(6), 0,
			0x7f, 0, pcm1690_dac_tlv),
	SOC_DOUBLE_R_TLV("Channel 7/8 Playback Volume",
			PCM1690_ATT_CONTROL(7), PCM1690_ATT_CONTROL(8), 0,
			0x7f, 0, pcm1690_dac_tlv),

  /*
   * Disabled because this requires snd_kcontrol->private_data to be set with pcm1690_private. It's not now, so it crashes.
   * I can't figure out how this is done. It looks like you can't use snd_soc_codec_driver.controls, but instead call 
   * 'struct snd_kcontrol * snd_soc_cnew(const struct snd_kcontrol_new * _template, void * data, const char * long_name, const char * prefix)'
   * manually, passing the data, but there are still pieces missing...
   *
	SOC_SINGLE_BOOL_EXT("De-emphasis Switch", 0,
			    pcm1690_get_deemph, pcm1690_put_deemph),
  */
};

static struct snd_soc_dai_driver pcm1690_dai = {
	.name = "pcm1690-hifi",
	.playback = {
		.stream_name = "Playback",
		.channels_min = 2,
		.channels_max = 8,
		.rates = PCM1690_PCM_RATES,
		.formats = PCM1690_PCM_FORMATS,
	},
	.ops = &pcm1690_dai_ops,
};

#ifdef CONFIG_OF
static const struct of_device_id pcm1690_dt_ids[] = {
	{ .compatible = "ti,pcm1690", },
	{ }
};
MODULE_DEVICE_TABLE(of, pcm1690_dt_ids);
#endif

static const struct regmap_config pcm1690_regmap = {
	.reg_bits		= 8,
	.val_bits		= 8,
	.max_register		= 0x4f,
	.reg_defaults		= pcm1690_reg_defaults,
	.num_reg_defaults	= ARRAY_SIZE(pcm1690_reg_defaults),
	.writeable_reg		= pcm1690_writeable_reg,
	.readable_reg		= pcm1690_accessible_reg,
};

static struct snd_soc_codec_driver soc_codec_dev_pcm1690 = {
	.controls		= pcm1690_controls,
	.num_controls		= ARRAY_SIZE(pcm1690_controls),
	.dapm_widgets		= pcm1690_dapm_widgets,
	.num_dapm_widgets	= ARRAY_SIZE(pcm1690_dapm_widgets),
	.dapm_routes		= pcm1690_dapm_routes,
	.num_dapm_routes	= ARRAY_SIZE(pcm1690_dapm_routes),
};

static const struct i2c_device_id pcm1690_i2c_id[] = {
	{"pcm1690", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, pcm1690_i2c_id);

static int pcm1690_i2c_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int ret;
	struct pcm1690_private *priv;

	priv = devm_kzalloc(&client->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	// Defaults
	priv->tdm_slots = 2;

	priv->regmap = devm_regmap_init_i2c(client, &pcm1690_regmap);
	if (IS_ERR(priv->regmap)) {
		ret = PTR_ERR(priv->regmap);
		dev_err(&client->dev, "Failed to create regmap: %d\n", ret);
		return ret;
	}

	i2c_set_clientdata(client, priv);

	return snd_soc_register_codec(&client->dev, &soc_codec_dev_pcm1690,
		&pcm1690_dai, 1);
}

static int pcm1690_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	return 0;
}

static struct i2c_driver pcm1690_i2c_driver = {
	.driver = {
		.name	= "pcm1690",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(pcm1690_dt_ids),
	},
	.id_table	= pcm1690_i2c_id,
	.probe		= pcm1690_i2c_probe,
	.remove		= pcm1690_i2c_remove,
};

module_i2c_driver(pcm1690_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments PCM1690 ALSA SoC Codec Driver");
MODULE_AUTHOR("Wiebe Cazemier <wiebe@halfgaar.net>");
MODULE_LICENSE("GPL");
