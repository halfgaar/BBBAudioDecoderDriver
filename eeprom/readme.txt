By writing this eeprom, the cap manager will detect it on boot and load the
firmware file '/lib/firmware/BBBAudioDecoder-00A0.dtbo'. It will also prevent
the universal cape manager from claiming the GPIOs.

However, you still need to adjust /boot/uEnv.txt. I commented out the bottom
line of these two:

  ##BeagleBone Black: HDMI (Audio/Video) disabled:
  dtb=am335x-boneblack-emmc-overlay.dtb

If you want to be able to load the dtbo file at runtime, you also need to
disable the universal cape manager. Check for this line and sjust:

  cmdline=coherent_pool=1M quiet cape_universal=disable

See the 'overlay' directory for installing the device tree overlay file.
