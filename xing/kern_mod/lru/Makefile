#
# Copyright (C) 2019 Yizhou Shan. All rights reserved.
#

obj-m := lru_ms.o

lru_ms-y := core.o proc.o

# Targets
all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	sync
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	sync
help:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) help
