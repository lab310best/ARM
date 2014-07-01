KRDIR=linux-2.6.12/
PWD=$(shell pwd)
obj-m:=hpi.o
all:
	make -C $(KRDIR) M=$(PWD) modules
	cd test && make	

.PHONY: clean
clean:
	-rm -rf *.o *.ko *.mod.c .*.cmd .tmp_versions
	cd test && make clean
distclean:
	@make clean
	rm -f tags *~

