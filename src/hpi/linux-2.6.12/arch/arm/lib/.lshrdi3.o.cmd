cmd_arch/arm/lib/lshrdi3.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/lib/.lshrdi3.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os     -fno-omit-frame-pointer -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -malignment-traps -msoft-float -Uarm -Wdeclaration-after-statement     -DKBUILD_BASENAME=lshrdi3 -DKBUILD_MODNAME=lshrdi3 -c -o arch/arm/lib/lshrdi3.o arch/arm/lib/lshrdi3.c

deps_arch/arm/lib/lshrdi3.o := \
  arch/arm/lib/lshrdi3.c \
  arch/arm/lib/gcclib.h \

arch/arm/lib/lshrdi3.o: $(deps_arch/arm/lib/lshrdi3.o)

$(deps_arch/arm/lib/lshrdi3.o):
