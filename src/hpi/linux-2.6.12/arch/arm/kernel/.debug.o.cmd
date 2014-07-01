cmd_arch/arm/kernel/debug.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/kernel/.debug.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -D__ASSEMBLY__ -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -msoft-float    -c -o arch/arm/kernel/debug.o arch/arm/kernel/debug.S

deps_arch/arm/kernel/debug.o := \
  arch/arm/kernel/debug.S \
    $(wildcard include/config/debug/icedcc.h) \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/linux/linkage.h \
  include/asm/linkage.h \
  include/asm/hardware.h \
  include/asm/arch/hardware.h \
    $(wildcard include/config/no/multiword/io.h) \
  include/asm/sizes.h \
  include/asm/arch/map.h \
  include/asm/arch/debug-macro.S \
    $(wildcard include/config/debug/s3c2410/uart.h) \
  include/asm/arch/regs-serial.h \
  include/asm/arch/regs-gpio.h \

arch/arm/kernel/debug.o: $(deps_arch/arm/kernel/debug.o)

$(deps_arch/arm/kernel/debug.o):
