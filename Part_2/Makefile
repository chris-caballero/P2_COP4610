obj-m += my_timer.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(shell pwd) modules

clean:
	rm -f *.o *.ko *.mod.* Module.* modules.*
