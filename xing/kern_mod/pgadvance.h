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

enum pgadvance_list_type {
	PGADVANCE_TYPE_ZERO,
	PGADVANCE_TYPE_NROMAL,

	NR_PGADVANCE_TYPES
};

struct pgadvance_list {
	int count;
	int watermark;
	int high;
	int batch;
	struct list_head head;
} ____cacheline_aligned;

struct pgadvance_set {
	struct pgadvance_list lists[NR_PGADVANCE_TYPES];
} ____cacheline_aligned;

struct pgadvancers_work_info {
	u32 request_cpu;
	u32 list_type;
} __packed;

#define NR_PERCPU_WORK 64
struct pgadvancers_work_pool {
	u64 bitmap;
	struct pgadvancers_work_info pool[NR_PERCPU_WORK];
} ____cacheline_aligned __packed;

#endif /* _PAGEADVANCE_H_ */
