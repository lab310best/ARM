cmd_arch/arm/boot/zImage := /usr/local/arm/3.4.1/bin/arm-linux-objcopy -O binary -R .note -R .comment -S  arch/arm/boot/compressed/vmlinux arch/arm/boot/zImage
