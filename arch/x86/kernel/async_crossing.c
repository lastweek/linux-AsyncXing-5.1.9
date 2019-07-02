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
#include <linux/pgadvance.h>

int default_dummy_asyncx_syscall(int cmd, struct async_crossing_info __user * uinfo)
{
	return -ENOSYS;
}
EXPORT_SYMBOL(default_dummy_asyncx_syscall);

/*
 * This is post pgfault where everything has been handled already.
 * We used this during early stage of development.
 */
void default_dummy_asyncx_post_pgfault(struct pt_regs *regs,
				       unsigned long address)
{

}
EXPORT_SYMBOL(default_dummy_asyncx_post_pgfault);

/*
 * This is called before any actual pgfault handling.
 */
enum intercept_result
default_dummy_intercept(struct pt_regs *regs, struct vm_area_struct *vma,
			unsigned long address, unsigned int flags)
{
	return ASYNCX_PGFAULT_NOT_INTERCEPTED;
}
EXPORT_SYMBOL(default_dummy_intercept);

void default_dummy_measure_crossing_latency(struct pt_regs *regs)
{

}
EXPORT_SYMBOL(default_dummy_measure_crossing_latency);

/* Constant, used when user unregister */
static const struct asyncx_callbacks acb_default = {
	.syscall			= default_dummy_asyncx_syscall,
	.intercept_do_page_fault	= default_dummy_intercept,
	.post_pgfault_callback		= default_dummy_asyncx_post_pgfault,
	.measure_crossing_latency	= default_dummy_measure_crossing_latency,
};

/* Used during runtime */
struct asyncx_callbacks acb_live = {
	.syscall			= default_dummy_asyncx_syscall,
	.intercept_do_page_fault	= default_dummy_intercept,
	.post_pgfault_callback		= default_dummy_asyncx_post_pgfault,
	.measure_crossing_latency	= default_dummy_measure_crossing_latency,
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

/*
 * For page advance
 */

struct pgadvance_callbacks pcb_live;

int register_pgadvance_callbacks(struct pgadvance_callbacks *pcb)
{
	memcpy(&pcb_live, pcb, sizeof(struct pgadvance_callbacks));
	return 0;
}
EXPORT_SYMBOL(register_pgadvance_callbacks);

int unregister_pgadvance_callbacks(void)
{
	memset(&pcb_live, 0, sizeof(struct pgadvance_callbacks));
	return 0;
}
EXPORT_SYMBOL(unregister_pgadvance_callbacks);
