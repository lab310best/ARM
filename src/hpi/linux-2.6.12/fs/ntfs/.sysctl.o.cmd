cmd_fs/ntfs/sysctl.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,fs/ntfs/.sysctl.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os     -fno-omit-frame-pointer -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -malignment-traps -msoft-float -Uarm -Wdeclaration-after-statement  -DNTFS_VERSION=\"2.1.22\" -DNTFS_RW   -DKBUILD_BASENAME=sysctl -DKBUILD_MODNAME=ntfs -c -o fs/ntfs/sysctl.o fs/ntfs/sysctl.c

deps_fs/ntfs/sysctl.o := \
  fs/ntfs/sysctl.c \
    $(wildcard include/config/sysctl.h) \
    $(wildcard include/config/proc/fs.h) \

fs/ntfs/sysctl.o: $(deps_fs/ntfs/sysctl.o)

$(deps_fs/ntfs/sysctl.o):
