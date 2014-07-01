cmd_arch/arm/boot/compressed/misc.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/boot/compressed/.misc.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os     -fno-omit-frame-pointer -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -malignment-traps -msoft-float -Uarm -Wdeclaration-after-statement  -fpic -Dstatic=  -DKBUILD_BASENAME=misc -DKBUILD_MODNAME=misc -c -o arch/arm/boot/compressed/misc.o arch/arm/boot/compressed/misc.c

deps_arch/arm/boot/compressed/misc.o := \
  arch/arm/boot/compressed/misc.c \
    $(wildcard include/config/debug/icedcc.h) \
  include/linux/string.h \
  include/linux/compiler.h \
  include/linux/compiler-gcc3.h \
  include/linux/compiler-gcc.h \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/linux/posix_types.h \
  include/linux/stddef.h \
  include/asm/posix_types.h \
  include/asm/types.h \
  include/asm/string.h \
  include/asm/arch/uncompress.h \
    $(wildcard include/config/s3c2410/lowlevel/uart/port.h) \
    $(wildcard include/config/cpu/s3c2400.h) \
    $(wildcard include/config/s3c2410/boot/watchdog.h) \
  include/asm/arch/regs-serial.h \
  include/asm/arch/regs-gpio.h \
  include/asm/arch/regs-watchdog.h \
  include/asm/arch/map.h \
  arch/arm/boot/compressed/../../../../lib/inflate.c \

arch/arm/boot/compressed/misc.o: $(deps_arch/arm/boot/compressed/misc.o)

$(deps_arch/arm/boot/compressed/misc.o):
