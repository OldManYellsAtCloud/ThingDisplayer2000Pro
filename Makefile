ssd-i2c-objs := 7-segment.o 7-segment-i2c.o
ssd-spi-objs := 7-segment.o 7-segment-spi.o
obj-m += ssd-i2c.o
obj-m += ssd-spi.o

PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean


