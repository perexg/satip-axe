obj-m=i2c_mangle.o

all:
	make -C $(PWD)/kernel ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD)/tools modules

clean:
	make -C $(PWD)/kernel ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD)/tools clean
