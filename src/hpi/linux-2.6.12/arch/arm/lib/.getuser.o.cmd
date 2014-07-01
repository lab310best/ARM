cmd_arch/arm/lib/getuser.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,arch/arm/lib/.getuser.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -D__ASSEMBLY__ -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -msoft-float    -c -o arch/arm/lib/getuser.o arch/arm/lib/getuser.S

deps_arch/arm/lib/getuser.o := \
  arch/arm/lib/getuser.S \
  include/asm/constants.h \
  include/asm/thread_info.h \
  include/asm/fpstate.h \
    $(wildcard include/config/iwmmxt.h) \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/asm/errno.h \
  include/asm-generic/errno.h \
  include/asm-generic/errno-base.h \

arch/arm/lib/getuser.o: $(deps_arch/arm/lib/getuser.o)

$(deps_arch/arm/lib/getuser.o):
