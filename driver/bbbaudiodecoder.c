#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <linux/gpio.h>

#define RESET_ACTIVE_LOW 48
#define TEMP_TEST_LED 7

struct snd_soc_card_drvdata_bbb_audio_decoder {
  struct clk *mclk;
  unsigned sysclk;
};

static const struct snd_soc_dapm_widget pcm1690_dapm_widgets[] =
{
  SND_SOC_DAPM_LINE("Line Out", NULL),
};

static int snd_bbb_audio_decoder_init(struct snd_soc_pcm_runtime *rtd)
{
  struct snd_soc_card *card = rtd->card;
  struct device_node *np = card->dev->of_node;
  struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
  struct snd_soc_dai *dac_dai = rtd->codec_dai;
  int ret;
  unsigned int tdm_mask = 0x00;
  unsigned int rx_tdm_mask = 0xff;
  u32 cpu_to_dac_tdm_slots;

  dev_info(card->dev, "Init starting...");

  snd_soc_dapm_new_controls(&card->dapm, pcm1690_dapm_widgets, ARRAY_SIZE(pcm1690_dapm_widgets));

  if (!np)
  {
    dev_err(card->dev, "No device tree node loaded?");
    return -1;
  }

  ret = of_property_read_u32(np, "cpu-to-dac-tdm-slots", &cpu_to_dac_tdm_slots);
  if (ret)
  {
    dev_err(card->dev, "Unable to obtain 'cpu-to-dac-tdm-slots' from device tree");
    return ret;
  }

  if (cpu_to_dac_tdm_slots > 8 || cpu_to_dac_tdm_slots < 2 )
  {
    dev_err(card->dev, "cpu_to_dac_tdm_slots must be between 2 and 8");
    return -1;
  }

  tdm_mask = 0xFF;
  tdm_mask = tdm_mask >> (8 - cpu_to_dac_tdm_slots);
  rx_tdm_mask = rx_tdm_mask >> (8 - 2);

  ret = snd_soc_dai_set_tdm_slot(dac_dai, 0, 0, cpu_to_dac_tdm_slots, 32);
  if (ret < 0){
    dev_err(dac_dai->dev, "Unable to set pcm1690 TDM slots.\n");
    return ret;
  }

  // TDM setting for audio output, I think? Does this then configure the DAC with its sample format, or something?
  dev_info(card->dev, "Setting TDM slots on audio processor, for output, to %d", cpu_to_dac_tdm_slots);
  ret = snd_soc_dai_set_tdm_slot(cpu_dai, tdm_mask, rx_tdm_mask, cpu_to_dac_tdm_slots, 32);
  if (ret < 0)
  {
    dev_err(cpu_dai->dev, "Unable to set McASP TDM slots.\n");
    return ret;
  }

  dev_info(card->dev, "Card initialised.");

  return 0;
}

static int snd_bbb_audio_deocer_set_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
  int ret = 0;
  struct snd_soc_pcm_runtime *rtd = substream->private_data;
  struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
  struct snd_soc_card *soc_card = rtd->card;
  unsigned cpu_clock = ((struct snd_soc_card_drvdata_bbb_audio_decoder *) snd_soc_card_get_drvdata(soc_card))->sysclk;

  ret = snd_soc_dai_set_sysclk(cpu_dai, 0, cpu_clock, SND_SOC_CLOCK_OUT);
  if (ret < 0){
    dev_err(cpu_dai->dev, "Unable to set cpu dai sysclk: %d.\n", ret);
    return ret;
  }
  dev_dbg(cpu_dai->dev, "Set CPU DAI clock rate to %d.\n", cpu_clock);

  return 0;
}

static int snd_bbb_audio_decoder_stream_startup(struct snd_pcm_substream *substream)
{
  struct snd_soc_pcm_runtime *rtd = substream->private_data;
  struct snd_soc_card *soc_card = rtd->card;
  struct snd_soc_card_drvdata_bbb_audio_decoder *drvdata = snd_soc_card_get_drvdata(soc_card);

  if (drvdata->mclk)
    return clk_prepare_enable(drvdata->mclk);

  return 0;
}

static void snd_bbb_audio_decoder_stream_shutdown(struct snd_pcm_substream *substream)
{
  struct snd_soc_pcm_runtime *rtd = substream->private_data;
  struct snd_soc_card *soc_card = rtd->card;
  struct snd_soc_card_drvdata_bbb_audio_decoder *drvdata = snd_soc_card_get_drvdata(soc_card);

  if (drvdata->mclk)
    clk_disable_unprepare(drvdata->mclk);
}

static struct snd_soc_ops snd_bbb_audio_decoder_ops = 
{
  .hw_params = snd_bbb_audio_deocer_set_hw_params,
  .startup = snd_bbb_audio_decoder_stream_startup,
  .shutdown = snd_bbb_audio_decoder_stream_shutdown
};

// Apparently, the mcasp platform driver has rxclk and txclk async in I2S mode? I need that, for my two different chips.
#define BBB_AUDIO_DECODER_DAIFMT ( SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_IF| SND_SOC_DAIFMT_CBM_CFM )
static struct snd_soc_dai_link snd_bbb_audio_decoder_dai = 
{
  .name = "Halfgaar BBBAudioDecoder",
  .stream_name = "TDM",
  .codec_dai_name = "pcm1690-hifi",
  .dai_fmt = BBB_AUDIO_DECODER_DAIFMT,
  .ops = &snd_bbb_audio_decoder_ops,
  .init = snd_bbb_audio_decoder_init
};

static const struct of_device_id snd_bbb_audio_decoder_dt_ids[] = 
{
  {
    .compatible = "halfgaar,bbbaudiodecoder",
    .data = &snd_bbb_audio_decoder_dai
  },
  { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, snd_bbb_audio_decoder_dt_ids);

static struct snd_soc_card snd_bbb_audio_decoder_card =
{
  .owner = THIS_MODULE,
  .num_links = 1,
};


static int snd_bbb_audio_decoder_probe(struct platform_device *pdev)
{
  struct device_node *np = pdev->dev.of_node;
  const struct of_device_id *match = of_match_device(of_match_ptr(snd_bbb_audio_decoder_dt_ids), &pdev->dev);
  struct snd_soc_dai_link *dai = (struct snd_soc_dai_link *) match->data;
  struct snd_soc_card_drvdata_bbb_audio_decoder *drvdata = NULL;
  struct clk *mclk;
  int ret = 0;

  snd_bbb_audio_decoder_card.dai_link = dai;

  dai->codec_of_node = of_parse_phandle(np, "audio-codec", 0);
  if (!dai->codec_of_node)
    return -EINVAL;

  dai->cpu_of_node = of_parse_phandle(np, "mcasp-controller", 0);
  if (!dai->cpu_of_node)
    return -EINVAL;

  dai->platform_of_node = dai->cpu_of_node;

  snd_bbb_audio_decoder_card.dev = &pdev->dev;
  ret = snd_soc_of_parse_card_name(&snd_bbb_audio_decoder_card, "model");
  if (ret)
    return ret;

  mclk = devm_clk_get(&pdev->dev, "mclk");
  if (PTR_ERR(mclk) == -EPROBE_DEFER)
  {
    return -EPROBE_DEFER;
  }
  else if (IS_ERR(mclk))
  {
    dev_dbg(&pdev->dev, "mclk not found.\n");
    mclk = NULL;
  }

  drvdata = devm_kzalloc(&pdev->dev, sizeof(*drvdata), GFP_KERNEL);
  if (!drvdata)
    return -ENOMEM;

  drvdata->mclk = mclk;

  ret = of_property_read_u32(np, "cpu-clock-rate", &drvdata->sysclk);
  if (ret < 0)
  {
    if (!drvdata->mclk)
    {
      dev_err(&pdev->dev, "No clock or clock rate defined.\n");
      return -EINVAL;
    }
    drvdata->sysclk = clk_get_rate(drvdata->mclk);
  }
  else if (drvdata->mclk)
  {
    unsigned int requestd_rate = drvdata->sysclk;
    clk_set_rate(drvdata->mclk, drvdata->sysclk);
    drvdata->sysclk = clk_get_rate(drvdata->mclk);
    if (drvdata->sysclk != requestd_rate)
      dev_warn(&pdev->dev, "Could not get requested rate %u using %u.\n", requestd_rate, drvdata->sysclk);
  }

  ret = gpio_request(RESET_ACTIVE_LOW, "bbb_reset_active_low");
  if (ret != 0 )
    return ret;
  ret = gpio_request(TEMP_TEST_LED, "temp_test_led");
  if (ret != 0 )
    return ret;

  // Setting 0-to-1 manually because the output seems to float at half supply
  // voltage if I set to 1 directly failing to turn on the DIR9001
  gpio_direction_output(RESET_ACTIVE_LOW, 0);
  gpio_set_value(RESET_ACTIVE_LOW, 1);

  gpio_direction_output(TEMP_TEST_LED, 1);

  dev_info(&pdev->dev, "About to register card");
  snd_soc_card_set_drvdata(&snd_bbb_audio_decoder_card, drvdata);
  ret = devm_snd_soc_register_card(&pdev->dev, &snd_bbb_audio_decoder_card);
  if (ret)
    dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);

  return ret;
}

static int snd_bbb_audio_decoder_remove(struct platform_device *pdev)
{
  gpio_set_value(RESET_ACTIVE_LOW, 0);
  gpio_set_value(TEMP_TEST_LED, 0);
  gpio_free(RESET_ACTIVE_LOW);
  gpio_free(TEMP_TEST_LED);

  return snd_soc_unregister_card(&snd_bbb_audio_decoder_card);

  return 0;
}

static struct platform_driver snd_bbb_audio_decoder_driver = 
{
  .driver = 
  {
    .name = "snd_bbb_audio_decoder",
    .pm = &snd_soc_pm_ops,
    .of_match_table = of_match_ptr(snd_bbb_audio_decoder_dt_ids)
  },
  .probe = snd_bbb_audio_decoder_probe,
  .remove = snd_bbb_audio_decoder_remove
};

module_platform_driver(snd_bbb_audio_decoder_driver);

MODULE_AUTHOR("Wiebe Cazemier");
MODULE_DESCRIPTION("Halfgaar's BBB Audio Decoder");
MODULE_LICENSE("GPL");
