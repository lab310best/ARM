cmd_arch/arm/kernel/vmlinux.lds := /usr/local/arm/3.4.1/bin/arm-linux-gcc -E -Wp,-MD,arch/arm/kernel/.vmlinux.lds.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian  -DTEXTADDR=0xC0008000 -DDATAADDR= -P -C -Uarm -D__ASSEMBLY__ -o arch/arm/kernel/vmlinux.lds arch/arm/kernel/vmlinux.lds.S

deps_arch/arm/kernel/vmlinux.lds := \
  arch/arm/kernel/vmlinux.lds.S \
    $(wildcard include/config/xip/kernel.h) \
  include/asm-generic/vmlinux.lds.h \
  include/linux/config.h \
    $(wildcard include/config/h.h) \
  include/asm/thread_info.h \
  include/asm/fpstate.h \
    $(wildcard include/config/iwmmxt.h) \

arch/arm/kernel/vmlinux.lds: $(deps_arch/arm/kernel/vmlinux.lds)

$(deps_arch/arm/kernel/vmlinux.lds):
