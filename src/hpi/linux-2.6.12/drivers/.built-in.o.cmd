cmd_drivers/built-in.o :=  /usr/local/arm/3.4.1/bin/arm-linux-ld -EL   -r -o drivers/built-in.o drivers/video/built-in.o drivers/char/built-in.o drivers/input/serio/built-in.o drivers/serial/built-in.o drivers/base/built-in.o drivers/block/built-in.o drivers/misc/built-in.o drivers/net/built-in.o drivers/media/built-in.o drivers/ide/built-in.o drivers/scsi/built-in.o drivers/cdrom/built-in.o drivers/mtd/built-in.o drivers/usb/built-in.o drivers/input/built-in.o drivers/i2c/built-in.o drivers/bluetooth/built-in.o drivers/mmc/built-in.o drivers/firmware/built-in.o drivers/crypto/built-in.o