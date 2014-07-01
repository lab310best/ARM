cmd_drivers/char/canbus/spi.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,drivers/char/canbus/.spi.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os     -fno-omit-frame-pointer -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -malignment-traps -msoft-float -Uarm -Wdeclaration-after-statement     -DKBUILD_BASENAME=spi -DKBUILD_MODNAME=mcp -c -o drivers/char/canbus/spi.o drivers/char/canbus/spi.c

deps_drivers/char/canbus/spi.o := \
  drivers/char/canbus/spi.c \
  include/asm-arm/arch-s3c2410/regs-gpio.h \
  include/asm-arm/arch-s3c2410/map.h \
  include/asm-arm/arch-s3c2410/regs-irq.h \
  include/linux/delay.h \
  include/asm/delay.h \
  include/asm-arm/io.h \
  include/linux/types.h \
    $(wildcard include/config/uid16.h) \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/linux/posix_types.h \
  include/linux/stddef.h \
  include/linux/compiler.h \
  include/linux/compiler-gcc3.h \
  include/linux/compiler-gcc.h \
  include/asm/posix_types.h \
  include/asm/types.h \
  include/asm/byteorder.h \
  include/linux/byteorder/little_endian.h \
  include/linux/byteorder/swab.h \
  include/linux/byteorder/generic.h \
  include/asm/memory.h \
    $(wildcard include/config/discontigmem.h) \
  include/asm/arch/memory.h \
    $(wildcard include/config/cpu/s3c2400.h) \
  include/asm/arch/hardware.h \
    $(wildcard include/config/no/multiword/io.h) \
  include/asm/sizes.h \
  include/asm/arch/map.h \
  include/asm/arch/io.h \
  include/asm-arm/arch-s3c2410/regs-clock.h \
    $(wildcard include/config/cpu/s3c2440.h) \
  drivers/char/canbus/def.h \
  drivers/char/canbus/spi.h \
  drivers/char/canbus/option.h \

drivers/char/canbus/spi.o: $(deps_drivers/char/canbus/spi.o)

$(deps_drivers/char/canbus/spi.o):
