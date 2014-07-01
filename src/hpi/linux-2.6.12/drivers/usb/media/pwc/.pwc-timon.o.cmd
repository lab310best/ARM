cmd_drivers/usb/media/pwc/pwc-timon.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,drivers/usb/media/pwc/.pwc-timon.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os     -fno-omit-frame-pointer -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -malignment-traps -msoft-float -Uarm -Wdeclaration-after-statement     -DKBUILD_BASENAME=pwc_timon -DKBUILD_MODNAME=pwc -c -o drivers/usb/media/pwc/pwc-timon.o drivers/usb/media/pwc/pwc-timon.c

deps_drivers/usb/media/pwc/pwc-timon.o := \
  drivers/usb/media/pwc/pwc-timon.c \
  drivers/usb/media/pwc/pwc-timon.h \
  drivers/usb/media/pwc/pwc-ioctl.h \

drivers/usb/media/pwc/pwc-timon.o: $(deps_drivers/usb/media/pwc/pwc-timon.o)

$(deps_drivers/usb/media/pwc/pwc-timon.o):
