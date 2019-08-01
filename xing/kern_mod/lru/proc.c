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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "core.h"

DEFINE_PER_CPU(struct stats, lru_stats);

void sum_stats(unsigned long *ret)
{
	int cpu;
	int i;

	memset(ret, 0, NR_LRU_STAT_ITEMS * sizeof(unsigned long));

	for_each_online_cpu(cpu) {
		struct stats *this = &per_cpu(lru_stats, cpu);

		for (i = 0; i < NR_LRU_STAT_ITEMS; i++)
			ret[i] += this->stats[i];
	}
}

const char *const lru_stat_text[] = {
	"nr_add_cache",
	"nr_add_falbk",
	"nr_drain",
	"nr_klrums_run",
};

static int lru_proc_show(struct seq_file *f, void *v)
{
	int cpu, i;
	unsigned long sum[NR_LRU_STAT_ITEMS];

	sum_stats(sum);

	seq_printf(f, "[Global Stats]\n");
	seq_printf(f, "     %-30s %10s\n", "Events", "Sum");
	for (i = 0; i < NR_LRU_STAT_ITEMS; i++) {
		seq_printf(f, "[%2d] %-30s %10lu\n",
			i, lru_stat_text[i], sum[i]);
	}
	seq_printf(f, "\n");

	seq_printf(f, "[Per-CPU Stats]\n");
	for_each_online_cpu(cpu) {
		struct stats *this = &per_cpu(lru_stats, cpu);

		seq_printf(f, "CPU%2d", cpu);
		for (i = 0; i < NR_LRU_STAT_ITEMS; i++) {
			seq_printf(f, "    %10lu", this->stats[i]);
		}
		seq_printf(f, "\n");
	}

	return 0;
}

static int lru_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, lru_proc_show, NULL);
}

static ssize_t lru_proc_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *offs)
{
	return -EIO;
}

static const struct file_operations lru_proc_fops = {
	.open		=	lru_proc_open,
	.read		=	seq_read,
	.write		=	lru_proc_write,
	.llseek		=	seq_lseek,
	.release	=	single_release
};

static struct proc_dir_entry *proc_entry;

int create_proc_files(void)
{
	proc_entry = proc_create("lrums", 0, NULL, &lru_proc_fops);
	if (!proc_entry)
		return -EIO;
	return 0;
}

void remove_proc_files(void)
{
	if (proc_entry)
		remove_proc_entry("lrums", NULL);
}
