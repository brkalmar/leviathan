KERNELRELEASE = $(shell uname -r)

obj-m += kraken_x61.o
kraken_x61-objs := src/kraken_x61/main.o
kraken_x61-objs += src/common.o

obj-m += kraken_x62.o
kraken_x62-objs := src/kraken_x62/main.o
kraken_x62-objs += src/kraken_x62/led.o
kraken_x62-objs += src/kraken_x62/percent.o
kraken_x62-objs += src/kraken_x62/status.o
kraken_x62-objs += src/common.o
kraken_x62-objs += src/util.o

all:
	$(MAKE) -C /lib/modules/$(KERNELRELEASE)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(KERNELRELEASE)/build M=$(PWD) clean
