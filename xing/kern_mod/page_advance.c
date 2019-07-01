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
#include <linux/percpu.h>
#include "page_advance.h"
#include "../config.h"

struct pgadvance_set *pas __percpu;

static int pgadvance_init(void)
{
	pas = alloc_percpu(struct pgadvance_set);
	if (!pas)
		return -ENOMEM;

	return 0;
}

static void pgadvance_exit(void)
{
	if (pas)
		free_percpu(pas);
}

module_init(pgadvance_init);
module_exit(pgadvance_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
