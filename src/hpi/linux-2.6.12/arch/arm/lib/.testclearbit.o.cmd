cmd_arch/arm/lib/testclearbit.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/lib/.testclearbit.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -D__ASSEMBLY__ -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -msoft-float    -c -o arch/arm/lib/testclearbit.o arch/arm/lib/testclearbit.S

deps_arch/arm/lib/testclearbit.o := \
  arch/arm/lib/testclearbit.S \
  include/linux/linkage.h \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/asm/linkage.h \
  include/asm/assembler.h \
  include/asm/ptrace.h \
    $(wildcard include/config/arm/thumb.h) \
    $(wildcard include/config/smp.h) \
  arch/arm/lib/bitops.h \

arch/arm/lib/testclearbit.o: $(deps_arch/arm/lib/testclearbit.o)

$(deps_arch/arm/lib/testclearbit.o):
