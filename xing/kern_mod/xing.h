/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _ASYNC_XING_MODULE_H_
#define _ASYNC_XING_MODULE_H_

#include <linux/kthread.h>
//#include <linux/async_crossing.h>
#include "../../include/linux/async_crossing.h"

/*
 * Information passed from the faulting cpu to the handling CPU.
 * async_crossing_info contains everything, and it can be derived
 * from tsk. @address is the faulting user virtual address.
 */
struct asyncx_delegate_info {
	unsigned long flags;
	struct task_struct *tsk;
	unsigned long address;
};

int init_asyncx_thread(void);
void exit_asyncx_thread(void);

#endif /* _ASYNC_XING_MODULE_H_ */
