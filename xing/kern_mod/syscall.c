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
#include <linux/kthread.h>
#include <linux/kprobes.h>
#include "xing.h"

static int handle_asyncx_set(struct async_crossing_info __user * uinfo)
{
	struct async_crossing_info *kinfo;
	struct page *page;
	int ret;

	kinfo = kmalloc(sizeof(*kinfo), GFP_KERNEL);
	if (!kinfo)
		return -ENOMEM;

	if (copy_from_user(kinfo, uinfo, sizeof(struct async_crossing_info))) {
		kfree(kinfo);
		return -EFAULT;
	}

	ret = get_user_pages(kinfo->shared_pages, 1, 0, &page, NULL);
	kinfo->p_shared_pages = page;
	kinfo->kva_shared_pages = page_to_virt(page);

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

int cb_syscall(int cmd, struct async_crossing_info __user * uinfo)
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
