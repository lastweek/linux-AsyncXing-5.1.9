/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>
#include <linux/pgadvance.h>
#include "pgadvance.h"
#include "../config.h"

struct pgadvance_set *pas __percpu;
struct task_struct **pgadvancers __percpu;

static void refill_list(struct pgadvance_list *list, int cpu,
			enum pgadvance_list_type type,
			int nr_to_fill)
{
	int i;
	struct page *page;
	gfp_t flags = GFP_KERNEL;

	pr_info("%s(): %d-%s %#lx nr:%d cpu:%d\n",
		__func__, current->pid, current->comm, (unsigned long)list, nr_to_fill, cpu);

	if (type == PGADVANCE_TYPE_ZERO)
		flags |= __GFP_ZERO;

	for (i = 0; i < nr_to_fill; i++) {
		page = alloc_page(flags);
		if (!page) {
			WARN_ONCE(1, "Fail to alloc");
			break;
		}
		list_add(&page->lru, &list->head);
	}
	list->count += i;
}

static struct page *cb_alloc_zero_page(void)
{
#if 0
	struct pgadvance_set *p = this_cpu(pas);
	struct pgadvance_list *l = &p->lists[PGADVANCE_TYPE_ZERO];
	struct page *page;

	if (unlikely(!l->count))
		refill_list(l, smp_processor_id(), PGADVANCE_TYPE_ZERO, l->batch);	

	page = list_first_entry(&l->head, struct page, lru);
	list_del(&page->lru);
	l->count--;
	return page;
#endif
	return alloc_page(__GFP_ZERO);
}

struct pgadvance_callbacks pcb = {
	.alloc_zero_page = cb_alloc_zero_page
};

/* Free all pages of a given list */
static void free_list(struct pgadvance_list *list)
{
	struct page *page;

	while (!list_empty(&list->head)) {
		page = list_first_entry(&list->head, struct page, lru);
		list_del(&page->lru);
		put_page(page);
	}
}

static int cal_watermark(void)
{
	return 128;
}

static int cal_batch(void)
{
	return 32;
}

/*
 * Allocate percpu data structure and alloc pages
 * to fill all the lists.
 */
static int init_percpu_sets(void)
{
	enum pgadvance_list_type type;
	int cpu, watermark, batch;

	pas = alloc_percpu_gfp(struct pgadvance_set, __GFP_ZERO);
	if (!pas)
		return -ENOMEM;

	watermark = cal_watermark();
	batch = cal_batch();

	for_each_possible_cpu(cpu) {
		struct pgadvance_set *p = per_cpu_ptr(pas, cpu);
		struct pgadvance_list *l;

		pr_crit("%s(): %#lx\n", __func__, p);
		for (type = 0; type < NR_PGADVANCE_TYPES; type++) {
			l = &p->lists[type];

			l->count = 0;
			l->watermark = watermark;
			l->batch = batch;
			INIT_LIST_HEAD(&l->head);
			refill_list(l, cpu, type, watermark);
		}
	}
	return 0;
}

static void free_percpu_sets(void)
{
	int cpu;
	enum pgadvance_list_type type;

	if (!pas)
		return;

	for_each_possible_cpu(cpu) {
		struct pgadvance_set *p = per_cpu_ptr(pas, cpu);
		struct pgadvance_list *l;

		pr_crit("%s(): %#lx\n", __func__, p);
		for (type = 0; type < NR_PGADVANCE_TYPES; type++) {
			l = &p->lists[type];
			free_list(l);
		}
	}
	free_percpu(pas);
}

static int pgadvancers_func(void *_unused)
{
	pr_crit("%s CPU %d run\n", current->comm, smp_processor_id());
	while (1) {
		schedule();
		if (kthread_should_stop())
			break;
	}
	pr_crit("%s CPU %d exit\n", current->comm, smp_processor_id());

	return 0;
}

static void exit_pgadvance_threads(void)
{
	int cpu;

	if (!pgadvancers)
		return;

	for_each_possible_cpu(cpu) {
		struct task_struct *tsk = per_cpu_ptr(pgadvancers, cpu);
		if (!IS_ERR_OR_NULL(tsk))
			kthread_stop(tsk);
	}
}

/* Create per-cpu pgadvancer thread and pin them. */
static int init_pgadvance_threads(void)
{
	int cpu;
	struct task_struct *tsk;

	pgadvancers = alloc_percpu_gfp(struct task_struct *, __GFP_ZERO);
	if (!pgadvancers)
		return -ENOMEM;

	for_each_possible_cpu(cpu) {
		tsk = kthread_create(pgadvancers_func, NULL, "pgadvancers%d", cpu);
		if (IS_ERR(tsk)) {
			pr_err("Fail to create pgadvancer for CPU %d\n", cpu);
			return -EFAULT;
		}
		kthread_bind(tsk, cpu);
		wake_up_process(tsk);

		per_cpu(pgadvancers, cpu) = tsk;
	}
	return 0;
}

static int pgadvance_init(void)
{
	int ret;

	ret = init_percpu_sets();
	if (ret)
		return ret;

	ret = init_pgadvance_threads();
	if (ret) {
		free_percpu_sets();
		return ret;
	}

	ret = register_pgadvance_callbacks(&pcb);
	if (ret) {
		exit_pgadvance_threads();
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
	exit_pgadvance_threads();
	free_percpu_sets();
}

module_init(pgadvance_init);
module_exit(pgadvance_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
