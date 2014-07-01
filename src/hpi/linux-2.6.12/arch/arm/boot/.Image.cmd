cmd_arch/arm/boot/Image := /usr/local/arm/3.4.1/bin/arm-linux-objcopy -O binary -R .note -R .comment -S  vmlinux arch/arm/boot/Image
