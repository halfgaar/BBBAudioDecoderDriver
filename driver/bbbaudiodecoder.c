#include <linux/module.h>
#include <linux/platform_device.h>
#include <sound/core.h>
#include <sound/soc.h>

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

static struct snd_soc_dai_link snd_bbb_audio_decoder_dai = 
{
  .name = "Halfgaar BBBAudioDecoder",
  .stream_name = "TDM",
  .codec_dai_name = "pcm1690-hifi",
  .dai_fmt = SND_SOC_DAIFMT_I2S,
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


static int snd_bbb_audio_decoder_probe(struct platform_device *pdev)
{
  return 0;
}

static int snd_bbb_audio_decoder_remove(struct platform_device *pdev)
{
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
