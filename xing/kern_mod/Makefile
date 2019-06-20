#
# Copyright (C) 2019 Yizhou Shan. All rights reserved.
#

obj-m := async_crossing.o

async_crossing-y := core.o thread.o

# Targets
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
help:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) help
