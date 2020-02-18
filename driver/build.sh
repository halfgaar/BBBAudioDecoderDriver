#!/bin/bash -e

if [ -z "$BBB_AUDIO_DECODER_IP" ]; then
  echo "Define BBB_AUDIO_DECODER_IP somewhere, in your .bash_aliases or something"
  exit 1
fi

HOST="$BBB_AUDIO_DECODER_IP"

export PATH="/home/halfgaar/notbackedup/ti-sdk/ti-processor-sdk-linux-am335x-evm-03.03.00.04/linux-devkit/sysroots/x86_64-arago-linux/usr/bin:$PATH"

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-

scp pcm1690.ko "root@$HOST:/lib/modules/4.4.30-ti-r64/kernel/sound/soc/codecs/"
scp bbbaudiodecoder.ko "root@$HOST:/lib/modules/4.4.30-ti-r64/kernel/sound/soc/davinci/"
ssh "root@$HOST" 'depmod -a'
