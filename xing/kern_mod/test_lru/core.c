/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * This program is intended for measuring lru allocator perf.
 */

#include <linux/sched/clock.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>
#include <linux/pagevec.h>
#include <linux/pagemap.h>

static void print_lat(int order, unsigned long *lat, int nr_entries, gfp_t flag)
{
	int i;

	for (i = 0; i < nr_entries; i++) {
		pr_crit("idx=%-8d order=%-8d latency=%-8lu CPU=%d\n",
			i, order, lat[i], smp_processor_id());
		schedule();
	}
}

atomic_t wait = { 0 };
int nr_threads = 6;

int param_nr_run = 10000;
module_param(param_nr_run, int, 0);

static int eval_func(void *_unused)
{
	struct page **pages;
	unsigned long *lat, start, end;
	size_t size;
	int i, ret;

	pr_crit("Running on CPU%d\n", smp_processor_id());

	/* Prepare data structures */
	size = param_nr_run * sizeof(*pages);
	pages = vmalloc(size);
	if (!pages)
		return -ENOMEM;
	memset(pages, 0, size);

	size = param_nr_run * sizeof(*lat);
	lat = vmalloc(size);
	if (!lat) {
		vfree(pages);
		return -ENOMEM;
	}
	memset(lat, 0, size);

	/* Allocate pages beforehand */
	for (i = 0; i < param_nr_run; i++) {
		struct page *p;

		/*
		 * XXX does gfp flag matter?
		 */
		p = alloc_pages_node(numa_mem_id(), __GFP_MOVABLE, 0);
		if (!p) {
			ret = -ENOMEM;
			goto free_nolru_pages;
		}
		pages[i] = p;
	}

	/* A raw barrier */
	atomic_inc(&wait);
	while (atomic_read(&wait) != nr_threads) {
		cpu_relax();
		if (signal_pending(current)) {
			ret = -EINTR;
			goto free_nolru_pages;
		}
	}

	for (i = 0; i < param_nr_run; i++) {
		struct page *p = pages[i];

		start = rdtsc_ordered();
		__lru_cache_add(p);
		end = rdtsc_ordered();
		lat[i] = end - start;
	}

	set_current_state(TASK_UNINTERRUPTIBLE);
	schedule_timeout(3 * HZ);
	release_pages(pages, param_nr_run);

	print_lat(0, lat, param_nr_run, 0);

	ret = 0;
out:
	vfree(pages);
	vfree(lat);
	return ret;

free_nolru_pages:
	for (i = 0; i < param_nr_run; i++) {
		if (pages[i]) {
			__free_pages(pages[i], 0);
		}
	}
	goto out;
}

static int eval_lru_init(void)
{
	struct task_struct *tsk;

	tsk = kthread_create(eval_func, NULL, "eva11");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 23);
	wake_up_process(tsk);

	tsk = kthread_create(eval_func, NULL, "eva22");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 21);
	wake_up_process(tsk);

	tsk = kthread_create(eval_func, NULL, "eva3");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 19);
	wake_up_process(tsk);

	tsk = kthread_create(eval_func, NULL, "eva4");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 17);
	wake_up_process(tsk);

#if 1
	tsk = kthread_create(eval_func, NULL, "eva5");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 15);
	wake_up_process(tsk);

	tsk = kthread_create(eval_func, NULL, "eva6");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 13);
	wake_up_process(tsk);
#endif

	return 0;
}

static void eval_lru_exit(void)
{

}

module_init(eval_lru_init);
module_exit(eval_lru_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
