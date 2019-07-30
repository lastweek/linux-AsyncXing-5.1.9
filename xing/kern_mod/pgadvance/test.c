/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/sched/clock.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>

#include "pgadvance.h"

/*
 * [0, param_max_order)
 */
int param_max_order = 1;
int param_alloc_per_order = 10000;

static void print_lat(int order, unsigned long *lat, int nr_entries, gfp_t flag)
{
	int i;
	unsigned long total = 0;

	for (i = 0; i < nr_entries; i++) {
		total += lat[i];
		pr_info("pdidx=%-8d order=%-8d latency=%-8lu CPU=%d\n",
			i, order, lat[i], smp_processor_id());
	}

	pr_crit("Average: %lu\n", total / nr_entries);
}

int eval_func(void)
{
	struct page **pages;
	unsigned long *lat, start, end;
	size_t size;
	int i, nr_order;
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

	for (nr_order = 0; nr_order < param_max_order; nr_order++) {
		for (i = 0; i < param_alloc_per_order; i++) {
			struct page *p;

			//start = sched_clock();
			start = rdtsc_ordered();
			p = pcb.alloc_zero_page(__GFP_MOVABLE);
			end = rdtsc_ordered();
			//end = sched_clock();

			mdelay(1);
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

	vfree(pages);
	vfree(lat);

	return 0;
}
