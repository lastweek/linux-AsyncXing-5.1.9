/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _LINUX_ASYNC_CROSSING_H_
#define _LINUX_ASYNC_CROSSING_H_

#include <uapi/linux/async_crossing.h>

/*
 * APIs for Kernel Modules
 */
struct asyncx_callbacks {
	int	(*syscall)(int, struct async_crossing_info __user *);
	void	(*post_pgfault_callback)(struct pt_regs *, unsigned long);
};

extern struct asyncx_callbacks acb_live;

int register_asyncx_callbacks(struct asyncx_callbacks *cb);
int unregister_asyncx_callbacks(struct asyncx_callbacks *cb);

void asyncx_post_pgfault(struct pt_regs *regs, unsigned long address);

#endif /* _LINUX_ASYNC_CROSSING_H_ */
