cmd_arch/arm/lib/io-writesb.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/lib/.io-writesb.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -D__ASSEMBLY__ -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -msoft-float    -c -o arch/arm/lib/io-writesb.o arch/arm/lib/io-writesb.S

deps_arch/arm/lib/io-writesb.o := \
  arch/arm/lib/io-writesb.S \
  include/linux/linkage.h \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/asm/linkage.h \
  include/asm/assembler.h \
  include/asm/ptrace.h \
    $(wildcard include/config/arm/thumb.h) \
    $(wildcard include/config/smp.h) \

arch/arm/lib/io-writesb.o: $(deps_arch/arm/lib/io-writesb.o)

$(deps_arch/arm/lib/io-writesb.o):
