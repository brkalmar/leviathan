obj-m += kraken.o
kraken-objs := src/kraken/main.o
kraken-objs += src/common.o

obj-m += kraken_x62.o
kraken_x62-objs := src/kraken_x62/main.o
kraken_x62-objs += src/kraken_x62/dynamic.o
kraken_x62-objs += src/kraken_x62/led.o
kraken_x62-objs += src/kraken_x62/percent.o
kraken_x62-objs += src/kraken_x62/status.o
kraken_x62-objs += src/common.o
kraken_x62-objs += src/util.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
