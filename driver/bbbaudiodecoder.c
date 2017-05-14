#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <linux/gpio.h>

#define CLOCK_ENABLE_LINE 59 // GPIO1_27. Needs to be 1 on BBB to enable the oscillator output on GPIO3_21
#define RESET_ACTIVE_LOW 48
#define TEMP_TEST_LED 7

static int snd_bbb_audio_decoder_init(struct snd_soc_pcm_runtime *rtd)
{
  return 0;
}

static int snd_bbb_audio_deocer_set_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
  return 0;
}

static int snd_bbb_audio_decoder_stream_startup(struct snd_pcm_substream *substream)
{
  return 0;
}

static void snd_bbb_audio_decoder_stream_shutdown(struct snd_pcm_substream *substream)
{

}

static struct snd_soc_ops snd_bbb_audio_decoder_ops = 
{
  .hw_params = snd_bbb_audio_deocer_set_hw_params,
  .startup = snd_bbb_audio_decoder_stream_startup,
  .shutdown = snd_bbb_audio_decoder_stream_shutdown
};

// Apparently, the mcasp platform driver has rxclk and txclk async in I2S mode? I need that, for my two different chips.
// TODO: research polarity (SND_SOC_DAIFMT_NB_NF?) and i2s mode.
#define BBB_AUDIO_DECODER_DAIFMT ( SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS )
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

  // Enable the I2s ahclkx master clock (at hardware-fixed 24576000 Hz) and set the RST pin high to activate the chips.
  ret = gpio_request(CLOCK_ENABLE_LINE, "ahclkx_enable");
  if (ret != 0 )
    return ret;
  ret = gpio_request(RESET_ACTIVE_LOW, "bbb_reset_active_low");
  if (ret != 0 )
    return ret;
  ret = gpio_request(TEMP_TEST_LED, "temp_test_led");
  if (ret != 0 )
    return ret;
  gpio_direction_output(CLOCK_ENABLE_LINE, 1);
  gpio_direction_output(RESET_ACTIVE_LOW, 1);
  gpio_direction_output(TEMP_TEST_LED, 1);

  dev_info(&pdev->dev, "About to register card");
  ret = devm_snd_soc_register_card(&pdev->dev, &snd_bbb_audio_decoder_card);
  if (ret)
    dev_err(&pdev->dev, "snd_soc_register_card failed (%d)\n", ret);

  return ret;
}

static int snd_bbb_audio_decoder_remove(struct platform_device *pdev)
{
  gpio_set_value(CLOCK_ENABLE_LINE, 0);
  gpio_set_value(RESET_ACTIVE_LOW, 0);
  gpio_set_value(TEMP_TEST_LED, 0);
  gpio_free(CLOCK_ENABLE_LINE);
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
    /*.pm = &snd_soc_pm_ops,*/
    .of_match_table = of_match_ptr(snd_bbb_audio_decoder_dt_ids)
  },
  .probe = snd_bbb_audio_decoder_probe,
  .remove = snd_bbb_audio_decoder_remove
};

module_platform_driver(snd_bbb_audio_decoder_driver);

MODULE_AUTHOR("Wiebe Cazemier");
MODULE_DESCRIPTION("Halfgaar's BBB Audio Decoder");
MODULE_LICENSE("GPL");
