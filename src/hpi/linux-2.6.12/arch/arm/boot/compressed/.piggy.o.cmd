cmd_arch/arm/boot/compressed/piggy.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/boot/compressed/.piggy.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -D__ASSEMBLY__ -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -msoft-float    -c -o arch/arm/boot/compressed/piggy.o arch/arm/boot/compressed/piggy.S

deps_arch/arm/boot/compressed/piggy.o := \
  arch/arm/boot/compressed/piggy.S \

arch/arm/boot/compressed/piggy.o: $(deps_arch/arm/boot/compressed/piggy.o)

$(deps_arch/arm/boot/compressed/piggy.o):
