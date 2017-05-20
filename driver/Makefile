obj-m+=pcm1690.o
obj-m+=bbbaudiodecoder.o

MY_CFLAGS += -g -DDEBUG

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
debug:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules EXTRA_CFLAGS="$(MY_CFLAGS)"
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
