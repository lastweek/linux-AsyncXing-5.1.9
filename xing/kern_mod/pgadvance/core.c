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
#include <linux/gfp.h>
#include "pgadvance.h"
#include "../../config.h"

DEFINE_PER_CPU_ALIGNED(struct pgadvance_set, pas);
DEFINE_PER_CPU_ALIGNED(struct pgadvancers_work_pool, pgadvancers_work_pool);
DEFINE_PER_CPU(struct task_struct *, pgadvancers);

static inline void list_del_nodebug(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static inline struct page *dequeue_page(struct sublist *sl)
{
	struct page *page;

	spin_lock(&sl->lock);
	page = list_first_entry(&sl->head, struct page, lru);
	list_del_nodebug(&page->lru);
	sl->count--;
	spin_unlock(&sl->lock);
	return page;
}

static inline void
enqueue_pages(struct sublist *sl, struct list_head *new_pages, int nr)
{
	spin_lock(&sl->lock);
	list_splice(new_pages, &sl->head);
	sl->count += nr;
	spin_unlock(&sl->lock);
}

static inline void enqueue_page(struct sublist *sl, struct page *new)
{
	spin_lock(&sl->lock);
	list_add(&new->lru, &sl->head);
	sl->count++;
	spin_unlock(&sl->lock);
}

static void prep_new_page(struct page *page)
{
	set_page_private(page, 0);
	set_page_count(page, 1);
}

/*
 * This function intercept the last step of freeing a page.
 * Since the pages inside our lists are in "allocated" state,
 * thus we must take the steps to prepare a new page.
 *
 * page->index was set to migrate type already.
 */
static void cb_free_one_page(void *_page, int migrate_type)
{
	struct pgadvance_set *p = this_cpu_ptr(&pas);
	struct pgadvance_list *l = &p->lists[PGADVANCE_TYPE_ZERO];
	struct sublist *sl;
	struct page *page = _page;

	/*
	 * Note in current impl, the migrate_type was checked
	 * thus it MUST within PCPTYPEs range. We have this check
	 * here to catch any future modifications.
	 */
	if (unlikely(migrate_type >= MIGRATE_PCPTYPES))
		BUG();

	sl = &l->lists[migrate_type];
	prep_new_page(page);

	memset(page_to_virt(page), 0, PAGE_SIZE);
	enqueue_page(sl, page);
	inc_stat(NR_FREE);
}

#define GFP_MOVABLE_MASK (__GFP_RECLAIMABLE|__GFP_MOVABLE)
#define GFP_MOVABLE_SHIFT 3
static inline int __gfpflags_to_migratetype(gfp_t gfp_flags)
{
	return (gfp_flags & GFP_MOVABLE_MASK) >> GFP_MOVABLE_SHIFT;
}
#undef GFP_MOVABLE_MASK
#undef GFP_MOVABLE_SHIFT

/*
 * Callback for do_anonymous_page()
 */
static struct page *cb_alloc_zero_page(gfp_t flags)
{
	struct pgadvance_set *p = this_cpu_ptr(&pas);
	struct pgadvance_list *l = &p->lists[PGADVANCE_TYPE_ZERO];
	struct page *page;
	int migrate_type;
	struct sublist *sl;

	migrate_type = __gfpflags_to_migratetype(flags);
	if (unlikely(migrate_type >= MIGRATE_PCPTYPES)) {
		pr_crit_once("%s(): type %d not supported\n",
			__func__, migrate_type);
		return alloc_page(flags | __GFP_ZERO);
	}

	sl = &l->lists[migrate_type];
	if (unlikely(!sl->count)) {
		inc_stat(NR_SYNC_REFILL_ZERO);
		return alloc_page(flags | __GFP_ZERO);
	}

	inc_stat(NR_PAGES_ALLOC_ZERO);
	page = dequeue_page(sl);
	return page;
}

static struct page *cb_alloc_normal_page(gfp_t flags)
{
	struct pgadvance_set *p = this_cpu_ptr(&pas);
	struct pgadvance_list *l = &p->lists[PGADVANCE_TYPE_NORMAL];
	struct page *page;
	int migrate_type;
	struct sublist *sl;

	migrate_type = __gfpflags_to_migratetype(flags);
	if (unlikely(migrate_type >= MIGRATE_PCPTYPES)) {
		pr_crit_once("%s(): type %d not supported\n",
			__func__, migrate_type);
		return alloc_page(flags | __GFP_ZERO);
	}

	sl = &l->lists[migrate_type];
	if (unlikely(!sl->count)) {
		inc_stat(NR_SYNC_REFILL_NORMAL);
		return alloc_page(flags | __GFP_ZERO);
	}

	inc_stat(NR_PAGES_ALLOC_NORMAL);
	page = dequeue_page(sl);
	return page;
}

struct pgadvance_callbacks pcb = {
	.alloc_zero_page	= cb_alloc_zero_page,
	.alloc_normal_page	= cb_alloc_normal_page,
	.free_one_page		= cb_free_one_page,
};

static int cal_high(void)
{
	return 1024;
}

static int cal_watermark(void)
{
	return 512;
}

static gfp_t refill_flags[] = {
	GFP_KERNEL,
	__GFP_MOVABLE,
	__GFP_RECLAIMABLE,
};

static void
refill_sublist(struct sublist *sl, enum migratetype mt, int cpu,
	       enum pgadvance_list_type list_type, unsigned int nr_to_fill)
{
	int i, node;
	struct page *page;
	gfp_t flags;
	LIST_HEAD(new_pages);

	node = cpu_to_node(cpu);
	nr_to_fill = min(nr_to_fill, sl->high);

	flags = refill_flags[mt];
	if (list_type == PGADVANCE_TYPE_ZERO)
		flags |= __GFP_ZERO;

	for (i = 0; i < nr_to_fill; i++) {
		page = __alloc_pages_nodemask(flags, 0, node, NULL);
		if (!page) {
			WARN_ON_ONCE(1);
			break;
		}
		__list_add(&page->lru, &new_pages, (&new_pages)->next);
	}

	inc_stat(NR_REFILLS_COMPLETED);
	enqueue_pages(sl, &new_pages, i);
}

DEFINE_PER_CPU(int, rr_counter);
int nr_total_cpus;

/*
 * TODO:
 * Need to choose a light-loaded CPU.
 * Based on what? Candidates:
 * - sched load
 * - our own stats
 */
static inline int choose_refill_cpu(void)
{
	int cpu;

retry:
	cpu = this_cpu_read(rr_counter) % nr_total_cpus;
	this_cpu_inc(rr_counter);
	if (unlikely(cpu == smp_processor_id()))
		goto retry;
	cpu = 23;
	return cpu;
}

int pgadvance_wakeup_interval_s = 1;

/* The background daemon */
static int pgadvancers_func(void *_unused)
{
	struct pgadvancers_work_pool *wpool;
	int current_cpu, current_node;

	pr_info("%s running on CPU %d\n", current->comm, smp_processor_id());

	wpool = this_cpu_ptr(&pgadvancers_work_pool);
	current_cpu = smp_processor_id();
	current_node = numa_node_id();

	while (1) {
		int cpu;

		for_each_online_cpu(cpu) {
			struct pgadvance_set *p = per_cpu_ptr(&pas, cpu);
			struct pgadvance_list *l;
			struct sublist *sl;
			int list_type, mt, nr_refill;

			if (cpu == 22 || cpu == 23)
				continue;

			if (cpu_to_node(cpu) != current_node)
				continue;

			for (list_type = 0; list_type < NR_PGADVANCE_TYPES; list_type++) {
				l = &p->lists[list_type];
				for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
					sl = &l->lists[mt];

					if (sl->count < sl->watermark) {
						nr_refill = sl->high - sl->count;
						refill_sublist(sl, mt, cpu, list_type, nr_refill);
					}
				}
			}
		}
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(1);
		//schedule();
		if (kthread_should_stop())
			break;
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
		if (cpu == 23 || cpu == 22) {
			tsk = kthread_create(pgadvancers_func, NULL, "kpgadvancer/%d", cpu);
			if (IS_ERR(tsk))
				return -EFAULT;
			kthread_bind(tsk, cpu);
			wake_up_process(tsk);

			per_cpu(pgadvancers, cpu) = tsk;

			wpool = per_cpu_ptr(&pgadvancers_work_pool, cpu);
			memset(wpool, 0, sizeof(*wpool));
		}
	}
	return 0;
}

/* Free all pages of a given list */
static void free_list(struct list_head *head)
{
	struct page *page;

	while (!list_empty(head)) {
		page = list_first_entry(head, struct page, lru);
		list_del(&page->lru);
		put_page(page);
	}
}

static void free_percpu_sets(void)
{
	int cpu, mt;
	enum pgadvance_list_type type;

	for_each_possible_cpu(cpu) {
		struct pgadvance_set *p = per_cpu_ptr(&pas, cpu);
		struct pgadvance_list *l;
		struct sublist *sl;

		for (type = 0; type < NR_PGADVANCE_TYPES; type++) {
			l = &p->lists[type];
			for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
				sl = &l->lists[mt];
				free_list(&sl->head);
			}
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
	int mt, cpu, watermark, high;

	watermark = cal_watermark();
	high = cal_high();

	for_each_possible_cpu(cpu) {
		struct pgadvance_set *p = per_cpu_ptr(&pas, cpu);
		struct pgadvance_list *l;
		struct sublist *sl;

		for (type = 0; type < NR_PGADVANCE_TYPES; type++) {
			l = &p->lists[type];
			for (mt = 0; mt < MIGRATE_PCPTYPES; mt++) {
				sl = &l->lists[mt];
				sl->count = 0;
				sl->high = high;
				sl->watermark = watermark;
				spin_lock_init(&sl->lock);
				INIT_LIST_HEAD(&sl->head);
				refill_sublist(sl, mt, cpu, type, sl->high);
			}
		}
	}
	return 0;
}

static int pgadvance_init(void)
{
	int ret;

	nr_total_cpus = num_online_cpus();

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

	ret = create_proc_files();
	if (ret) {
		unregister_pgadvance_callbacks();
		exit_pgadvance_threads();
		free_percpu_sets();
		return ret;
	}

	set_cpu_active(22, false);
	set_cpu_active(23, false);

	set_cpu_active(10, false);
	set_cpu_active(11, false);
	return 0;
}

static void pgadvance_exit(void)
{
	//eval_func();

	/*
	 * Unregister first then deallocate resources
	 * otherwise it might intermingle.
	 */
	remove_proc_files();
	unregister_pgadvance_callbacks();
	exit_pgadvance_threads();
	free_percpu_sets();

	set_cpu_active(22, true);
	set_cpu_active(23, true);
	set_cpu_active(10, true);
	set_cpu_active(11, true);
}

module_init(pgadvance_init);
module_exit(pgadvance_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
