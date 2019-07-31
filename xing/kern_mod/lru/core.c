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
#include <linux/gfp.h>
#include <linux/pagevec.h>
#include <linux/pagevec_ms.h>
#include <linux/memcontrol.h>
#include <linux/swap.h>

/*
 * Add a page into its LRU vector.
 * This is similar to the original __lru_cache_add().
 */
static void add_page_to_lru(struct page *page)
{
	struct pglist_data *pgdat;
	struct lruvec *lruvec;
	unsigned long flags;

	pgdat = page_pgdat(page);
	spin_lock_irqsave(&pgdat->lru_lock, flags);
	lruvec = mem_cgroup_page_lruvec(page, pgdat);
	__pagevec_lru_add_fn(page, lruvec, NULL);
	spin_unlock_irqrestore(&pgdat->lru_lock, flags);
}

/*
 * Drain cached pages of @cpu into their LRU vector
 */
static inline void drain_lru_ms_cpu(int cpu)
{
	struct pagevec_ms *pvec = per_cpu_ptr(&lru_add_ms, cpu);
	struct page *page;
	int count = 0;

	while (pagevec_ms_has_page(pvec)) {
		page = pagevec_ms_dequeue(pvec);
		add_page_to_lru(page);

		if (count++ >= NR_PAGEVEC_MS)
			break;
	}
}

static void cb__lru_add_drain_cpu(int cpu)
{
	drain_lru_ms_cpu(cpu);
}

/* Callback for the real __lru_cache_add() */
static void cb__lru_cache_add(struct page *page)
{
	struct pagevec_ms *pvec = &get_cpu_var(lru_add_ms);

	if (PageCompound(page) || pagevec_ms_full(pvec)) {
		struct pglist_data *pgdat;
		struct lruvec *lruvec;
		unsigned long flags;

		/*
		 * The cases where we directly insert the page
		 * into the lruvec.
		 */
		pgdat = page_pgdat(page);
		spin_lock_irqsave(&pgdat->lru_lock, flags);
		lruvec = mem_cgroup_page_lruvec(page, pgdat);
		__pagevec_lru_add_fn(page, lruvec, NULL);
		spin_unlock_irqrestore(&pgdat->lru_lock, flags);
	} else {
		get_page(page);
		pagevec_ms_enqueue(pvec, page);
	}
	put_cpu_var(lru_add_ms);
}

__used static void print_lru_ms(void)
{
	int cpu;

	for_each_online_cpu(cpu) {
		struct pagevec_ms *pvec = per_cpu_ptr(&lru_add_ms, cpu);

		pr_crit("CPU %d head %d tail %d  [%d - %d] nr=%d\n",
			cpu, pvec->head, pvec->tail,
			pvec->head % NR_PAGEVEC_MS,
			pvec->tail % NR_PAGEVEC_MS,
			pagevec_ms_nr_pages(pvec));
	}
}

static void reset_lru_ms(void)
{
	int cpu;
	for_each_online_cpu(cpu) {
		struct pagevec_ms *pvec = per_cpu_ptr(&lru_add_ms, cpu);
		pvec->head = 0;
		pvec->tail = 0;
	}
}

static void run_lru_ms(void)
{
	int cpu;
	for_each_online_cpu(cpu) {
		struct pagevec_ms *pvec = per_cpu_ptr(&lru_add_ms, cpu);
		struct page *page;

		while (pagevec_ms_has_page(pvec)) {
			page = pagevec_ms_dequeue(pvec);
			add_page_to_lru(page);
		}
	}
}

static int lru_microservice_func(void *_unused)
{
	lru_add_drain_all();
	lru_ms_registered = true;

	while (1) {
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
		run_lru_ms();
		if (kthread_should_stop())
			break;
	}

	lru_ms_registered = false;
	return 0;
}

static struct task_struct *lru_ms_task;

static int create_thread(void)
{
	struct task_struct *tsk;
	int cpu;

	tsk = kthread_create(lru_microservice_func, NULL, "kms_lru");
	if (IS_ERR(tsk))
		return PTR_ERR(tsk);

	cpu = 23;
	kthread_bind(tsk, cpu);
	wake_up_process(tsk);

	lru_ms_task = tsk;
	return 0;
}

struct lru_ms_callback cb = {
	.__lru_cache_add = cb__lru_cache_add,
	.lru_add_drain_cpu = cb__lru_add_drain_cpu,
};

static int lru_ms_init(void)
{
	int ret;

	reset_lru_ms();
	register_lru_ms_callback(&cb);

	ret = create_thread();
	if (ret) {
		pr_err("Fail to create thread");
		unregister_lru_ms_callback();
		return ret;
	}
	return 0;
}

static void destroy_thread(void)
{
	if (!lru_ms_task)
		return;

	kthread_stop(lru_ms_task);
}

static void drain_lru_ms_all(void)
{
	int cpu;
	for_each_online_cpu(cpu) {
		drain_lru_ms_cpu(cpu);
	}
}

static void lru_ms_exit(void)
{
	destroy_thread();

	/*
	 * Sequence matters.
	 * First unregister all callbacks,
	 * then release pages in our list.
	 */
	unregister_lru_ms_callback();
	drain_lru_ms_all();
	reset_lru_ms();
}

module_init(lru_ms_init);
module_exit(lru_ms_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
