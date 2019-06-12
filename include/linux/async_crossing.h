/*
 * Copyright (c) 2019, Yizhou Shan <syzwhat@gmail.com>. All rights reserved.
 * Subject to GPLv2.
 */
#ifndef _LINUX_ASYNC_CROSSING_H_
#define _LINUX_ASYNC_CROSSING_H_

#include <uapi/linux/async_crossing.h>

struct async_crossing_info {
	unsigned long	jmp_user_address;
	unsigned long	flags;
};

#endif /* _LINUX_ASYNC_CROSSING_H_ */
