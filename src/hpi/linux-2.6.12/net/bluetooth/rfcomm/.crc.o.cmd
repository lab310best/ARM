cmd_net/bluetooth/rfcomm/crc.o := /usr/local/arm/3.4.1/bin/arm-linux-gcc -Wp,-MD,net/bluetooth/rfcomm/.crc.o.d  -nostdinc -isystem /usr/local/arm/3.4.1/lib/gcc/arm-linux/3.4.1/include -D__KERNEL__ -Iinclude  -mlittle-endian -Wall -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -ffreestanding -Os     -fno-omit-frame-pointer -fno-omit-frame-pointer -mapcs -mno-sched-prolog -mapcs-32 -D__LINUX_ARM_ARCH__=4 -march=armv4 -mtune=arm9tdmi -malignment-traps -msoft-float -Uarm -Wdeclaration-after-statement     -DKBUILD_BASENAME=crc -DKBUILD_MODNAME=rfcomm -c -o net/bluetooth/rfcomm/crc.o net/bluetooth/rfcomm/crc.c

deps_net/bluetooth/rfcomm/crc.o := \
  net/bluetooth/rfcomm/crc.c \

net/bluetooth/rfcomm/crc.o: $(deps_net/bluetooth/rfcomm/crc.o)

$(deps_net/bluetooth/rfcomm/crc.o):
