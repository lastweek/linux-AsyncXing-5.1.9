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

static DEFINE_PER_CPU(struct pgadvance_set, pas);
static DEFINE_PER_CPU(struct task_struct *, pgadvancers);
static DEFINE_PER_CPU(struct pgadvancers_work_pool, pgadvancers_work_pool);

static void refill_list(struct pgadvance_list *list, int cpu,
			enum pgadvance_list_type type,
			int nr_to_fill)
{
	int i;
	struct page *page;
	gfp_t flags = GFP_KERNEL;

	//pr_info("%s(): %d-%s %#lx nr:%d cpu:%d\n",
	//	__func__, current->pid, current->comm, (unsigned long)list, nr_to_fill, cpu);

	if (type == PGADVANCE_TYPE_ZERO)
		flags |= __GFP_ZERO;

	for (i = 0; i < nr_to_fill; i++) {
		page = alloc_page(flags);
		if (!page) {
			WARN_ON_ONCE(1);
			break;
		}
		list_add(&page->lru, &list->head);
	}
	list->count += i;
}

//XXX
static inline int choose_refill_cpu(void)
{
	return 21;
}

static inline void send_refill_work(enum pgadvance_list_type type)
{
	int refill_cpu;
	struct pgadvancers_work_pool *wpool;
	struct pgadvancers_work_info winfo;
	u64 *_bitmap;
	u64 old, new, ret;
	int index;

	refill_cpu = choose_refill_cpu();
	wpool = per_cpu_ptr(&pgadvancers_work_pool, refill_cpu);
	_bitmap = &wpool->bitmap;

	/* Reserve the slot */
	for (;;) {
		old = *_bitmap;
		index = ffz(old);

		new = old | (1ULL << index);
		ret = cmpxchg(_bitmap, old, new);
		if (ret == old)
			break;

	}

	/* Then copy the work info */
	winfo.request_cpu = smp_processor_id();
	winfo.list_type = type;
	memcpy(&wpool->pool[index], &winfo, sizeof(winfo));

	pr_crit("%s()-cpu%d: refill_cpu=%d, index=%d, bitmap=%#lx\n",
		__func__, smp_processor_id(), refill_cpu, index, *_bitmap);
}

/*
 * Callback for do_anonymous_page()
 */
static struct page *cb_alloc_zero_page(void)
{
	struct pgadvance_set *p = this_cpu_ptr(&pas);
	struct pgadvance_list *l = &p->lists[PGADVANCE_TYPE_ZERO];
	struct page *page;

	if (unlikely(!l->count)) {
		send_refill_work(PGADVANCE_TYPE_ZERO);
		refill_list(l, smp_processor_id(), PGADVANCE_TYPE_ZERO, l->batch);	
	}

	page = list_first_entry(&l->head, struct page, lru);
	list_del(&page->lru);
	l->count--;
	return page;
}

static struct pgadvance_callbacks pcb = {
	.alloc_zero_page = cb_alloc_zero_page
};

static int cal_watermark(void)
{
	return 128;
}

static int cal_batch(void)
{
	return 32;
}

static inline void do_handle_refill_work(struct pgadvancers_work_info *winfo)
{
	int request_cpu = winfo->request_cpu;
	enum pgadvance_list_type type = winfo->list_type;
	struct pgadvance_set *ps;
	struct pgadvance_list *pl;

	ps = per_cpu_ptr(&pas, request_cpu);
	pl = &ps->lists[type];

	refill_list(pl, request_cpu, type, pl->batch);
}

/*
 * Find a pending work by scanning bitmap.
 * We will reset the bitmap before do the actual dirty work.
 */
static inline void handle_refill_work(struct pgadvancers_work_pool *wpool)
{
	struct pgadvancers_work_info winfo;
	u64 *_bitmap = &wpool->bitmap;
	u64 old, new;
	int index;

	/*
	 * XXX
	 * Think about what's the correct way to get data
	 */
	for (;;) {
		old = *_bitmap;
		index = fls64(old);
		if (unlikely(!index)) {
			WARN_ON_ONCE(1);
			return;
		}
		index -= 1;

		/*
		 * Grab the work info before reset the bitmap,
		 * in case it got overrided immediately by another racing CPU.
		 */
		memcpy(&winfo, &wpool->pool[index], sizeof(winfo));
		new = old & ~(1ULL << index);
		new = cmpxchg(_bitmap, old, new);
		if (new == old)
			break;
	}
	//do_handle_refill_work(&winfo);

	pr_crit("%s(): index=%d request_cpu=%2d\n",
		__func__, index, winfo.request_cpu);
}

int pgadvance_wakeup_interval_s = 1;

/* The background daemon */
static int pgadvancers_func(void *_unused)
{
	struct pgadvancers_work_pool *wpool;

	wpool = this_cpu_ptr(&pgadvancers_work_pool);
	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(pgadvance_wakeup_interval_s * HZ);

		if (kthread_should_stop())
			break;

		while (wpool->bitmap) {
			//XXX probabaly check time to yield
			handle_refill_work(wpool);
		}
	}
	return 0;
}

static void exit_pgadvance_threads(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct task_struct *tsk = per_cpu(pgadvancers, cpu);
		if (!IS_ERR_OR_NULL(tsk))
			kthread_stop(tsk);
	}
}

/* Create per-cpu pgadvancer thread and pin them. */
static int init_pgadvance_threads(void)
{
	int cpu;
	struct task_struct *tsk;
	struct pgadvancers_work_pool *wpool;

	for_each_possible_cpu(cpu) {
		tsk = kthread_create(pgadvancers_func, NULL, "kpgadvancer/%d", cpu);
		if (IS_ERR(tsk))
			return -EFAULT;
		kthread_bind(tsk, cpu);
		wake_up_process(tsk);

		per_cpu(pgadvancers, cpu) = tsk;

		wpool = per_cpu_ptr(&pgadvancers_work_pool, cpu);
		memset(wpool, 0, sizeof(*wpool));
	}
	return 0;
}

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

static void free_percpu_sets(void)
{
	int cpu;
	enum pgadvance_list_type type;

	for_each_possible_cpu(cpu) {
		struct pgadvance_set *p = per_cpu_ptr(&pas, cpu);
		struct pgadvance_list *l;

		for (type = 0; type < NR_PGADVANCE_TYPES; type++) {
			l = &p->lists[type];
			free_list(l);
		}
	}
}

/*
 * Allocate percpu data structure and alloc pages
 * to fill all the lists.
 */
static int init_percpu_sets(void)
{
	enum pgadvance_list_type type;
	int cpu, watermark, batch;

	watermark = cal_watermark();
	batch = cal_batch();

	for_each_possible_cpu(cpu) {
		struct pgadvance_set *p = per_cpu_ptr(&pas, cpu);
		struct pgadvance_list *l;

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
