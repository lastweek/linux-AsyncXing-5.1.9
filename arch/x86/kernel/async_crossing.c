/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/user.h>
#include <linux/regset.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/async_crossing.h>

int default_dummy_asyncx_syscall(int cmd, struct async_crossing_info __user * uinfo)
{
	return -ENOSYS;
}
EXPORT_SYMBOL(default_dummy_asyncx_syscall);

void default_dummy_asyncx_post_pgfault(struct pt_regs *regs,
				       unsigned long address)
{

}
EXPORT_SYMBOL(default_dummy_asyncx_post_pgfault);

int default_dummy_intercept(struct vm_area_struct *vma,
			    unsigned long address, unsigned int flags)
{
	return 0;
}
EXPORT_SYMBOL(default_dummy_intercept);

/* Constant, used when user unregister */
static const struct asyncx_callbacks acb_default = {
	.syscall			= default_dummy_asyncx_syscall,
	.intercept_do_page_fault	= default_dummy_intercept,
	.post_pgfault_callback		= default_dummy_asyncx_post_pgfault,
};

/* Used during runtime */
struct asyncx_callbacks acb_live = {
	.syscall			= default_dummy_asyncx_syscall,
	.intercept_do_page_fault	= default_dummy_intercept,
	.post_pgfault_callback		= default_dummy_asyncx_post_pgfault,
};

SYSCALL_DEFINE2(async_crossing, int, cmd, struct async_crossing_info __user *, uinfo)
{
	return acb_live.syscall(cmd, uinfo);
}

int register_asyncx_callbacks(struct asyncx_callbacks *cb)
{
	memcpy(&acb_live, cb, sizeof(struct asyncx_callbacks));
	return 0;
}
EXPORT_SYMBOL(register_asyncx_callbacks);

int unregister_asyncx_callbacks(struct asyncx_callbacks *cb)
{
	memcpy(&acb_live, &acb_default, sizeof(struct asyncx_callbacks));
	return 0;
}
EXPORT_SYMBOL(unregister_asyncx_callbacks);
