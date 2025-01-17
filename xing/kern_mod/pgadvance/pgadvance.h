/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _PAGEADVANCE_H_
#define _PAGEADVANCE_H_

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/pgadvance.h>

enum pgadvance_list_type {
	PGADVANCE_TYPE_ZERO,
	PGADVANCE_TYPE_NORMAL,

	NR_PGADVANCE_TYPES
};

static inline char *list_type_name(enum pgadvance_list_type type)
{
	switch (type) {
	case PGADVANCE_TYPE_ZERO:	return "ZERO";
	case PGADVANCE_TYPE_NORMAL:	return "NORMAL";
	default:			return "UNKNOWN";
	};
	return "BUG";
}

#define PGADVANCE_LIST_FLAG_REQUESTED	0x1

struct sublist {
	unsigned int count;
	unsigned int watermark;
	unsigned int high;
	struct list_head head;
	spinlock_t lock;
};

struct pgadvance_list {
	struct sublist lists[MIGRATE_PCPTYPES];
} ____cacheline_aligned __packed;

/*
 * Top per-cpu data structures
 */
struct pgadvance_set {
	struct pgadvance_list lists[NR_PGADVANCE_TYPES];
} ____cacheline_aligned;

struct pgadvancers_work_info {
	u32 request_cpu;
	u32 list_type;
} __packed;

#define INTEGRITY_FLAG	(0x80000000)

#define NR_PERCPU_WORK 64
struct pgadvancers_work_pool {
	u64 bitmap;
	struct pgadvancers_work_info pool[NR_PERCPU_WORK];
} ____cacheline_aligned __packed;

extern DEFINE_PER_CPU(struct pgadvance_set, pas);
extern DEFINE_PER_CPU(struct task_struct *, pgadvancers);
extern DEFINE_PER_CPU(struct pgadvancers_work_pool, pgadvancers_work_pool);

int create_proc_files(void);
void remove_proc_files(void);

/*
 * Lightweight per-cpu counter inspired by vmstat.h
 */

enum pgadvance_stat_item {
	NR_PAGES_ALLOC_ZERO,
	NR_REQUEST_REFILL_ZERO,
	NR_SYNC_REFILL_ZERO,

	NR_PAGES_ALLOC_NORMAL,
	NR_REQUEST_REFILL_NORMAL,
	NR_SYNC_REFILL_NORMAL,

	NR_REFILLS_COMPLETED,

	NR_REQUEST_REFILL_FAILED,

	NR_FREE,

	NR_PGADVANCE_STAT_ITEMS
};

struct pgadvance_stats {
	unsigned long stats[NR_PGADVANCE_STAT_ITEMS];
};

void sum_pgadvance_stats(unsigned long *ret);
extern DEFINE_PER_CPU(struct pgadvance_stats, pgadvance_stats);
extern const char *const pgadvance_stat_text[];

static inline void inc_stat(enum pgadvance_stat_item item)
{
	this_cpu_inc(pgadvance_stats.stats[item]);
}

extern struct pgadvance_callbacks pcb;

int eval_func(void);

#endif /* _PAGEADVANCE_H_ */
