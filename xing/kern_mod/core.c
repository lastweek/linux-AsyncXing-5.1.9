/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kprobes.h>
#include "xing.h"

static void __used dump_aci(struct async_crossing_info *aci)
{
	pr_info("Dump ACI:");
	pr_info("  jmp_user_address: %#lx\n", aci->jmp_user_address);
	pr_info("  jmp_user_stack:   %#lx\n", aci->jmp_user_stack);
	pr_info("  shared_pages:     %#lx\n", aci->shared_pages);
}

/*
 * @regs: user registers upon fault
 * @address: the faulting virtual address
 */
static void cb_pgfault(struct pt_regs *regs, unsigned long address)
{
	struct async_crossing_info *aci;
	struct shared_page_meta *user_page;
	struct pt_regs *user_page_regs;

	/* Check if registered */
	aci = current->aci;
	if (unlikely(!aci))
		return;
	user_page = (void *)aci->shared_pages;
	user_page_regs = &user_page->regs;

	/* We don't do nested handling */
	if (unlikely(user_page->flags & ASYNCX_INTERCEPTED)) {
		pr_crit("Nested pgfault: %#lx\n", address);
		return;
	}

	/* Save orignal fault context */
	user_page->flags = ASYNCX_INTERCEPTED;
	memcpy(user_page_regs, regs, sizeof(struct pt_regs));

	/* Replace with user register IP and SP */
	regs->ip = aci->jmp_user_address;
	regs->sp = aci->jmp_user_stack;
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

static int cb_syscall(int cmd, struct async_crossing_info __user * uinfo)
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

static struct asyncx_callbacks cb = {
	.syscall		= cb_syscall,
	.post_pgfault_callback	= cb_pgfault,
};

static int core_init(void)
{
	int ret;

	ret = register_asyncx_callbacks(&cb);
	if (ret) {
		pr_err("Fail to register asyncx callbacks.");
		return ret;
	}
	return 0;
}

static void core_exit(void)
{
	unregister_asyncx_callbacks(&cb);
}

module_init(core_init);
module_exit(core_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
