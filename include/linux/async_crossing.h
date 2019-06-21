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

enum intercept_result {
	ASYNCX_PGFAULT_NOT_INTERCEPTED,
	ASYNCX_PGFAULT_INTERCEPTED,
};

/*
 * APIs for Kernel Modules
 * @syscall: replace the default syscall handling
 * @intercept: intercept do_page_fault()
 * @post_pgfault_callback: after handle_mm_fault(), for testing
 */
struct asyncx_callbacks {
	int		(*syscall)(int, struct async_crossing_info __user *);


	enum intercept_result
			(*intercept_do_page_fault)(struct pt_regs *regs,
						   struct vm_area_struct *vma,
						   unsigned long address,
						   unsigned int flags);

	void		(*post_pgfault_callback)(struct pt_regs *, unsigned long);
};

/* Public API for modules */
int register_asyncx_callbacks(struct asyncx_callbacks *cb);
int unregister_asyncx_callbacks(struct asyncx_callbacks *cb);

void default_dummy_asyncx_post_pgfault(struct pt_regs *regs,
				       unsigned long address);
int default_dummy_asyncx_syscall(int cmd, struct async_crossing_info __user * uinfo);

enum intercept_result
default_dummy_intercept(struct pt_regs *regs, struct vm_area_struct *vma,
			unsigned long address, unsigned int flags);

extern struct asyncx_callbacks acb_live;

/*
 * Called when pgfault has finished processing and prepared
 * to return to userspace.
 */
static inline void
asyncx_post_pgfault(struct pt_regs *regs, unsigned long address)
{
	acb_live.post_pgfault_callback(regs, address);
}

/*
 * Called to intercept the original do_page_fault()
 */
static inline int
asyncx_intercept_do_page_fault(struct pt_regs *regs,
			       struct vm_area_struct *vma,
			       unsigned long address,
			       unsigned int flags)
{
	return acb_live.intercept_do_page_fault(regs, vma, address, flags);
}

#endif /* _LINUX_ASYNC_CROSSING_H_ */
