/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#define _GNU_SOURCE

#include <sys/utsname.h>
#include <math.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <linux/unistd.h>
#include <assert.h>
#include <sched.h>
#include <asm/ptrace.h>
#include <stdbool.h>

#include "async.h"
#include "includeme.h"
#include "../../include/uapi/linux/async_crossing.h"

#include "../config.h"

static __attribute__((always_inline)) inline unsigned long current_stack_pointer(void)
{
	unsigned long sp;
	asm volatile (
		"movq %%rsp, %0\n"
		: "=r" (sp)
	);
	return sp;
}

static __attribute__((always_inline)) inline unsigned long rdtsc(void)
{
	unsigned long low, high;
	asm volatile("rdtscp" : "=a" (low), "=d" (high));
	return ((low) | (high) << 32);
}

#define NR_PAGES 1000000ULL

static void test_pgfault_latency(void)
{
	void *foo;
	long nr_size, i;
	struct timeval ts, te, result;
	unsigned long s_tsc, e_tsc;

	nr_size = NR_PAGES * PAGE_SIZE;
	foo = mmap(NULL, nr_size, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (!foo)
		die("fail to malloc");
	
	gettimeofday(&ts, NULL);

	asm volatile("mfence": : :"memory");
	s_tsc = rdtsc();
	for (i = 0; i < NR_PAGES; i++) {
		int *bar, cut;

		bar = foo + PAGE_SIZE * i;
		*bar = 100;
	}
	e_tsc = rdtsc();
	asm volatile("mfence": : :"memory");

	gettimeofday(&te, NULL);
	timeval_sub(&result, &te, &ts);

#if 1
	printf("   NR_PAGES: %d Total Runtime: %ld.%06ld (s) Avg: %lu (ns)\n", NR_PAGES,
		result.tv_sec, result.tv_usec,
		(result.tv_sec * 1000000000 + result.tv_usec * 1000) / NR_PAGES);
#endif

	printf("   Total cycles: %lu Avg : %lu (cycles)\n",
		e_tsc - s_tsc, (e_tsc - s_tsc) / NR_PAGES);
}

int main(void)
{
	int i, cpu, node, ret;
	unsigned long jmp_user_address;
	void *shared_pages, *jmp_user_stack;
	int nr_shared_pages, nr_stack_pages;

	setbuf(stdout, NULL);

	pin_cpu(23);
	getcpu(&cpu, &node);
	printf("Running on cpu: %d, node: %d\n", cpu, node);

	printf(" ** Normal\n");
	test_pgfault_latency();

	return 0;
}
