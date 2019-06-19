/*
 * Copyright (c) 2019, Yizhou Shan <syzwhat@gmail.com>. All rights reserved.
 * Subject to GPLv2.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/user.h>
#include <linux/regset.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/async_crossing.h>

/*
 * This is called at the end of pgfault handling.
 * @regs: user registers upon fault
 * @address: the faulting virtual address
 */
void setup_asyncx_jmp(struct pt_regs *regs, unsigned long address)
{
	struct async_crossing_info *aci;

	/* Check if registered */
	aci = current->aci;
	if (!aci)
		return;

	regs->ip = aci->jmp_user_address;
}

static int handle_asyncx_set(struct async_crossing_info __user * uinfo)
{
	struct async_crossing_info *kinfo;

	kinfo = kmalloc(sizeof(*kinfo), GFP_KERNEL);
	if (!kinfo)
		return -ENOMEM;

	if (copy_from_user(kinfo, uinfo, sizeof(struct async_crossing_info))) {
		kfree(kinfo);
		return -EFAULT;
	}

	current->aci = kinfo;
	return 0;
}

static int handle_asyncx_unset(struct async_crossing_info __user * uinfo)
{
	struct async_crossing_info kinfo = { };

	if (copy_from_user(&kinfo, uinfo, sizeof(struct async_crossing_info)))
		return -EFAULT;

	if (current->aci) {
		kfree(current->aci);
		current->aci = NULL;
	}
	return 0;
}

static int handle_asyncx_get(struct async_crossing_info __user * uinfo)
{
	struct async_crossing_info *aci;

	aci = current->aci;
	if (!aci)
		return -ENODEV;
	if (copy_to_user(uinfo, aci, sizeof(*aci)))
		return -EFAULT;
	return 0;
}

SYSCALL_DEFINE2(async_crossing, int, cmd, struct async_crossing_info __user *, uinfo)
{
	int ret = 0;

	switch (cmd) {
	case ASYNCX_SET:
		ret = handle_asyncx_set(uinfo);
		break;
	case ASYNCX_UNSET:
		ret = handle_asyncx_unset(uinfo);
		break;
	case ASYNCX_GET:
		ret = handle_asyncx_get(uinfo);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}
