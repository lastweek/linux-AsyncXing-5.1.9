/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _LRU_MS_CORE_H_
#define _LRU_MS_CORE_H_

#include <linux/compiler.h>
#include <linux/kernel.h>

int create_proc_files(void);
void remove_proc_files(void);

/*
 * Lightweight per-cpu counter inspired by vmstat.h
 */

enum stat_item {
	NR_ADD_CACHE,
	NR_ADD_FALLBACK,
	NR_DRAIN,
	NR_KLRUMS_RUN,

	NR_LRU_STAT_ITEMS,
};

struct stats {
	unsigned long stats[NR_LRU_STAT_ITEMS];
};

void sum_stats(unsigned long *ret);
extern DEFINE_PER_CPU(struct stats, lru_stats);
extern const char *const lru_stat_text[];

static inline void inc_stat(enum stat_item item)
{
	this_cpu_inc(lru_stats.stats[item]);
}

#endif /* _LRU_MS_CORE_H_ */
