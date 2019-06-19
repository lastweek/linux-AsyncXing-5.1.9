/*
 * Copyright (c) 2019, Yizhou Shan <syzwhat@gmail.com>. All rights reserved.
 * Subject to GPLv2.
 */

#ifndef _LINUX_UAPI_ASYNC_CROSSING_H_
#define _LINUX_UAPI_ASYNC_CROSSING_H_

enum async_crossing_cmd {
	ASYNCX_SET,
	ASYNCX_UNSET,
	ASYNCX_GET,
};

struct async_crossing_info {
	unsigned long	flags;
	unsigned long	jmp_user_address;
};

#endif /* _LINUX_UAPI_ASYNC_CROSSING_H_ */
