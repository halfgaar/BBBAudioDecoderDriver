For the kernel to load the driver automatically, you need to copy the compiled
drivers into the /lib/modules directory, like here:

  ./4.4.30-ti-r64/kernel/sound/soc/codecs/pcm1690.ko
  ./4.4.30-ti-r64/kernel/sound/soc/davinci/bbbaudiodecoder.ko

And then run 'depmod -a'

Adjust and run build.sh to cross-compile the drivers.
