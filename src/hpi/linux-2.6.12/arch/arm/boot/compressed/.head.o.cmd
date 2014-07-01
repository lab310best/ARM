cmd_arch/arm/boot/compressed/head.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/boot/compressed/.head.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -D__ASSEMBLY__ -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -msoft-float    -c -o arch/arm/boot/compressed/head.o arch/arm/boot/compressed/head.S

deps_arch/arm/boot/compressed/head.o := \
  arch/arm/boot/compressed/head.S \
    $(wildcard include/config/debug/icedcc.h) \
    $(wildcard include/config/footbridge.h) \
    $(wildcard include/config/arch/rpc.h) \
    $(wildcard include/config/arch/integrator.h) \
    $(wildcard include/config/arch/pxa.h) \
    $(wildcard include/config/arch/ixp4xx.h) \
    $(wildcard include/config/arch/ixp2000.h) \
    $(wildcard include/config/arch/lh7a40x.h) \
    $(wildcard include/config/arch/omap.h) \
    $(wildcard include/config/arch/sa1100.h) \
    $(wildcard include/config/debug/ll/ser3.h) \
    $(wildcard include/config/arch/iop331.h) \
    $(wildcard include/config/arch/s3c2410.h) \
    $(wildcard include/config/s3c2410/lowlevel/uart/port.h) \
    $(wildcard include/config/zboot/rom.h) \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/linux/linkage.h \
  include/asm/linkage.h \

arch/arm/boot/compressed/head.o: $(deps_arch/arm/boot/compressed/head.o)

$(deps_arch/arm/boot/compressed/head.o):
