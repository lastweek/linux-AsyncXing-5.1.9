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
#include "pgadvance.h"
#include "../../config.h"

static void show_pgadvance_list_info(struct seq_file *f,
				     enum pgadvance_list_type type)
{
	int cpu;
	struct pgadvance_set *p;
	struct pgadvance_list *l;

	seq_printf(f, "List: %s\n", list_type_name(type));
	seq_printf(f, "CPU    high    batch    watermark    count\n");
	for_each_possible_cpu(cpu) {
		p = per_cpu_ptr(&pas, cpu);
		l = &p->lists[type];
		seq_printf(f, "%3d    %4d    %5d    %9d    %5d\n",
			cpu, l->high, l->batch, l->watermark, l->count);
	}
	seq_printf(f, "\n");
}

static void show_pgadvance_set_info(struct seq_file *f)
{
	enum pgadvance_list_type i;

	for (i = 0; i < NR_PGADVANCE_TYPES; i++)
		show_pgadvance_list_info(f, i);
}

static void show_pgadvance_stats_sum(struct seq_file *f)
{
	int i;
	unsigned long stats[NR_PGADVANCE_STAT_ITEMS];

	sum_pgadvance_stats(stats);

	seq_printf(f, "[Global Stats]\n");
	seq_printf(f, "%-30s  %10s\n", "Events", "Sum");
	for (i = 0; i < NR_PGADVANCE_STAT_ITEMS; i++) {
		seq_printf(f, "%-30s %10lu\n",
			pgadvance_stat_text[i],
			stats[i]);
	}
	seq_printf(f, "\n");
}

static void show_pgadvance_stats_percpu(struct seq_file *f)
{
	int i, cpu;
	struct pgadvance_stats *this;

	seq_printf(f, "[Percpu Stats]\n");
	seq_printf(f, "%-30s  ", "Events");
	for_each_possible_cpu(cpu)
		seq_printf(f, "%10d ", cpu);
	seq_printf(f, "\n");

	for (i = 0; i < NR_PGADVANCE_STAT_ITEMS; i++) {
		seq_printf(f, "%-30s  ", pgadvance_stat_text[i]);

		for_each_possible_cpu(cpu) {
			this = &per_cpu(pgadvance_stats, cpu);
			seq_printf(f, "%10ld ", this->stats[i]);
		}
		seq_printf(f, "\n");
	}
	seq_printf(f, "\n");
}

static int pgadvance_proc_show(struct seq_file *f, void *v)
{
	show_pgadvance_set_info(f);
	show_pgadvance_stats_sum(f);
	show_pgadvance_stats_percpu(f);
	return 0;
}

static int pgadvance_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, pgadvance_proc_show, NULL);
}

static ssize_t pgadvance_proc_write(struct file *file, const char __user *buf,
				    size_t count, loff_t *offs)
{
	return -EIO;
}

static const struct file_operations pgadvance_proc_fops = {
	.open		=	pgadvance_proc_open,
	.read		=	seq_read,
	.write		=	pgadvance_proc_write,
	.llseek		=	seq_lseek,
	.release	=	single_release
};

static struct proc_dir_entry *proc_entry;

int create_proc_files(void)
{
	proc_entry = proc_create("pgadvance", 0, NULL, &pgadvance_proc_fops);
	if (!proc_entry)
		return -EIO;
	return 0;
}

void remove_proc_files(void)
{
	if (proc_entry)
		remove_proc_entry("pgadvance", NULL);
}
