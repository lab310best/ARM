cmd_drivers/block/built-in.o :=  /usr/local/arm/3.4.1/bin/arm-linux-ld -EL   -r -o drivers/block/built-in.o drivers/block/elevator.o drivers/block/ll_rw_blk.o drivers/block/ioctl.o drivers/block/genhd.o drivers/block/scsi_ioctl.o drivers/block/noop-iosched.o drivers/block/as-iosched.o drivers/block/deadline-iosched.o drivers/block/cfq-iosched.o drivers/block/rd.o drivers/block/loop.o drivers/block/nbd.o drivers/block/ub.o
