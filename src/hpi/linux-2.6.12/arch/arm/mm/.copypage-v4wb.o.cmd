cmd_arch/arm/mm/copypage-v4wb.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/mm/.copypage-v4wb.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -D__ASSEMBLY__ -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -msoft-float    -c -o arch/arm/mm/copypage-v4wb.o arch/arm/mm/copypage-v4wb.S

deps_arch/arm/mm/copypage-v4wb.o := \
  arch/arm/mm/copypage-v4wb.S \
  include/linux/linkage.h \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/asm/linkage.h \
  include/linux/init.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/hotplug.h) \
  include/linux/compiler.h \
  include/asm/constants.h \

arch/arm/mm/copypage-v4wb.o: $(deps_arch/arm/mm/copypage-v4wb.o)

$(deps_arch/arm/mm/copypage-v4wb.o):
