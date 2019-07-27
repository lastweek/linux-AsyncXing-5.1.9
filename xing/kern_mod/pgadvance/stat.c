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
#include "../../config.h"

DEFINE_PER_CPU(struct pgadvance_stats, pgadvance_stats);

void sum_pgadvance_stats(unsigned long *ret)
{
	int cpu;
	int i;

	memset(ret, 0, NR_PGADVANCE_STAT_ITEMS * sizeof(unsigned long));

	for_each_online_cpu(cpu) {
		struct pgadvance_stats *this = &per_cpu(pgadvance_stats, cpu);

		for (i = 0; i < NR_PGADVANCE_STAT_ITEMS; i++)
			ret[i] += this->stats[i];
	}
}

const char *const pgadvance_stat_text[] = {
	"nr_alloc_zero",
	"nr_request_refill_zero",
	"nr_sync_refill_zero",
	
	"nr_alloc_normal",
	"nr_request_refill_normal",
	"nr_sync_refill_normal",

	"nr_refills_completed",

	"nr_request_refill_fail",

	"nr_free",
};
