obj-m+=pcm1690.o
obj-m+=bbbaudiodecoder.o

MY_CFLAGS += -g -DDEBUG

all:
	make -C /home/halfgaar/notbackedup/beagleboard-linux M=$(PWD) modules
debug:
	make -C /home/halfgaar/notbackedup/beagleboard-linux M=$(PWD) modules EXTRA_CFLAGS="$(MY_CFLAGS)"
clean:
	make -C /home/halfgaar/notbackedup/beagleboard-linux M=$(PWD) clean
