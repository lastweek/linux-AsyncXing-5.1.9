#
# Copyright (C) 2019 Yizhou Shan. All rights reserved.
#

obj-m := asyncx.o

asyncx-y := core.o syscall.o

# Targets
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
help:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) help
