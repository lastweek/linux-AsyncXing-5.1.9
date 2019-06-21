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
 *
 * HACK! Don't exceed one cacheline size, which is 64B.
 */
struct asyncx_delegate_info {
	/* Faulting task */
	struct task_struct *tsk;

	struct vm_area_struct *vma;
	unsigned long address;
	unsigned int pgfault_flags;

	/* Internal flags */
	unsigned long flags;
};

int init_asyncx_thread(void);
void exit_asyncx_thread(void);

#endif /* _ASYNC_XING_MODULE_H_ */
