/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * This program is intended for measuring buddy allocator perf.
 */

#include <linux/sched/clock.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>

/*
 * [0, param_max_order)
 */
int param_max_order = 1;
int param_alloc_per_order = 1000;

module_param(param_max_order, int, 0);
module_param(param_alloc_per_order, int, 0);

static void print_lat(int order, unsigned long *lat, int nr_entries, gfp_t flag)
{
	int i;

	for (i = 0; i < nr_entries; i++)
		pr_info("idx=%-8d order=%-8d latency=%-8lu CPU=%d\n",
			i, order, lat[i], smp_processor_id());
}

//gfp_t gfp_flag_array[] = {GFP_KERNEL, __GFP_MOVABLE | GFP_USER};
//gfp_t gfp_flag_array[] = {__GFP_MOVABLE | GFP_USER};
gfp_t gfp_flag_array[] = {GFP_KERNEL};

atomic_t wait = { 0 };
int nr_threads = 1;

static int eval_func(void *_unused)
{
	struct page **pages;
	unsigned long *lat, start, end;
	size_t size;
	int i, nr_order, j;
	gfp_t gfp_flag;

	size = param_alloc_per_order * sizeof(*pages);
	pages = vmalloc(size);
	if (!pages)
		return -ENOMEM;
	memset(pages, 0, size);

	size = param_alloc_per_order * sizeof(*lat);
	lat = vmalloc(size);
	if (!lat) {
		vfree(pages);
		return -ENOMEM;
	}
	memset(lat, 0, size);

	atomic_inc(&wait);
	while (atomic_read(&wait) != nr_threads)
		schedule();

	pr_crit("Running on CPU%d\n", smp_processor_id());

	for (nr_order = 0; nr_order < param_max_order; nr_order++) {
		for (j = 0; j < ARRAY_SIZE(gfp_flag_array); j++) {

			/*
			 * Okay, we have order and gfp_t
			 * go for it
			 */
			gfp_flag = gfp_flag_array[j];
			for (i = 0; i < param_alloc_per_order; i++) {
				struct page *p;

				//start = sched_clock();
				start = rdtsc_ordered();
				p = alloc_pages_node(numa_mem_id(), gfp_flag, nr_order);
				end = rdtsc_ordered();
				//end = sched_clock();

				if (likely(p)) {

					/*
					 * XXX
					 * Having this or not affect perf.
					 */
					//void *pp = page_to_virt(p);
					//memset(pp, 0, PAGE_SIZE);

					pages[i] = p;
					lat[i] = end - start;
				} else {
					pages[i] = NULL;
					lat[i] = 0;
				}
			}

			for (i = 0; i < param_alloc_per_order; i++) {
				if (pages[i])
					__free_pages(pages[i], nr_order);
			}
			print_lat(nr_order, lat, param_alloc_per_order, gfp_flag);
		}
	}

	vfree(pages);
	vfree(lat);

	return 0;
}

static int eval_buddy_init(void)
{
	struct task_struct *tsk;

	tsk = kthread_create(eval_func, NULL, "eva11");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 23);
	wake_up_process(tsk);

#if 0
	tsk = kthread_create(eval_func, NULL, "eva22");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 21);
	wake_up_process(tsk);

	tsk = kthread_create(eval_func, NULL, "eva22");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 19);
	wake_up_process(tsk);

	tsk = kthread_create(eval_func, NULL, "eva22");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 17);
	wake_up_process(tsk);

	tsk = kthread_create(eval_func, NULL, "eva22");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 15);
	wake_up_process(tsk);

	tsk = kthread_create(eval_func, NULL, "eva22");
	if (IS_ERR(tsk))
		return -EFAULT;
	kthread_bind(tsk, 13);
	wake_up_process(tsk);
#endif

	return 0;
}

static void eval_buddy_exit(void)
{

}

module_init(eval_buddy_init);
module_exit(eval_buddy_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
