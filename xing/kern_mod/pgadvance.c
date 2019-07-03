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
#include <linux/pgadvance.h>
#include "pgadvance.h"
#include "../config.h"

struct pgadvance_set *pas __percpu;

static struct page *cb_alloc_zero_page(void)
{
	return alloc_page(__GFP_MOVABLE|__GFP_ZERO);
}

struct pgadvance_callbacks pcb = {
	.alloc_zero_page = cb_alloc_zero_page
};

/*
 * Refill a certain list to meet its watermark.
 */
static void refill_list(struct pgadvance_list *list, int cpu,
			enum pgadvance_list_type type)
{
	int batch = list->batch;
}

static int cal_watermark(void)
{
	return 128;
}

static int cal_batch(void)
{
	return 32;
}

static int init_percpu_sets(void)
{
	enum pgadvance_list_type type;
	int cpu, watermark, batch;

	pas = alloc_percpu(struct pgadvance_set);
	if (!pas)
		return -ENOMEM;

	watermark = cal_watermark();
	batch = cal_batch();

	for_each_possible_cpu(cpu) {
		struct pgadvance_set *p = per_cpu_ptr(pas, cpu);
		struct pgadvance_list *l;

		for (type = 0; type < NR_PGADVANCE_TYPES; type++) {
			l = &p->lists[type];

			l->count = 0;
			l->watermark = watermark;
			l->batch = batch;
			INIT_LIST_HEAD(&l->head);
			refill_list(l, cpu, type);
		}
	}
	return 0;
}

static void free_percpu_sets(void)
{
	if (!pas)
		return;
	free_percpu(pas);
}

static int pgadvance_init(void)
{
	int ret;

	ret = init_percpu_sets();
	if (ret)
		return ret;

	ret = register_pgadvance_callbacks(&pcb);
	if (ret) {
		free_percpu_sets();
		return ret;
	}
	return 0;
}

static void pgadvance_exit(void)
{
	/*
	 * Unregister first then deallocate resources
	 * otherwise it might intermingle.
	 */
	unregister_pgadvance_callbacks();
	free_percpu_sets();
}

module_init(pgadvance_init);
module_exit(pgadvance_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
