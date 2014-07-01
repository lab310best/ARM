cmd_drivers/video/logo/logo_linux_tft16.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,drivers/video/logo/.logo_linux_tft16.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os     -fno-omit-frame-pointer -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -malignment-traps -msoft-float -Uarm -Wdeclaration-after-statement     -DKBUILD_BASENAME=logo_linux_tft16 -DKBUILD_MODNAME=logo_linux_tft16 -c -o drivers/video/logo/logo_linux_tft16.o drivers/video/logo/logo_linux_tft16.c

deps_drivers/video/logo/logo_linux_tft16.o := \
  drivers/video/logo/logo_linux_tft16.c \
  include/linux/linux_logo.h \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/linux/compiler.h \
  include/linux/compiler-gcc3.h \
  include/linux/compiler-gcc.h \
  drivers/video/logo/tft16_logo.c \

drivers/video/logo/logo_linux_tft16.o: $(deps_drivers/video/logo/logo_linux_tft16.o)

$(deps_drivers/video/logo/logo_linux_tft16.o):
