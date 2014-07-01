cmd_.tmp_kallsyms1.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,./..tmp_kallsyms1.o.d -D__ASSEMBLY__ -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -msoft-float   -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian    -c -o .tmp_kallsyms1.o .tmp_kallsyms1.S

deps_.tmp_kallsyms1.o := \
  .tmp_kallsyms1.S \
  include/asm/types.h \

.tmp_kallsyms1.o: $(deps_.tmp_kallsyms1.o)

$(deps_.tmp_kallsyms1.o):
