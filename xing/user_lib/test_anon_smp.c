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

#include "includeme.h"

static __attribute__((always_inline)) inline unsigned long rdtsc(void)
{
	unsigned long low, high;
	asm volatile("rdtscp" : "=a" (low), "=d" (high));
	return ((low) | (high) << 32);
}

#define NR_PAGES 500000ULL

static pthread_barrier_t local_barrier;

static void *thread_func(void *_sum)
{
	void *foo;
	long nr_size, i;
	unsigned long s_tsc, e_tsc;
	unsigned long *sum = _sum;
	int cpu, node;

	getcpu(&cpu, &node);
	printf("    .. runs on cpu: %d, node: %d\n", cpu, node);

	nr_size = NR_PAGES * PAGE_SIZE;
	foo = mmap(NULL, nr_size, PROT_READ|PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (!foo)
		die("fail to malloc");

	pthread_barrier_wait(&local_barrier);

	asm volatile("mfence": : :"memory");
	s_tsc = rdtsc();
	for (i = 0; i < NR_PAGES; i++) {
		volatile int *bar, cut;

		bar = foo + PAGE_SIZE * i;
		*bar = 100;
		//cut = *bar;
	}
	e_tsc = rdtsc();
	asm volatile("mfence": : :"memory");

	*sum = e_tsc - s_tsc;
	printf("    ..avg %lu\n", *sum / NR_PAGES);
}

/*
 * Create multiple threads, each of them got an exclusive
 * mmaped area. Measure page fault latency.
 */
void test_pgfault_threads(int nr_threads)
{
	pthread_t *tid;
	unsigned long *tsc, total;
	int i, ret;

	printf(" ** Test **\n");

	tid = malloc(sizeof(*tid) * nr_threads);
	tsc = malloc(sizeof(*tsc) * nr_threads);
	if (!tid || !tsc)
		die("OOM");

	ret = pthread_barrier_init(&local_barrier, NULL, nr_threads);
	if (ret)
		die("Fail to init barrier");

	for (i = 0; i < nr_threads; i++) {
		ret = pthread_create(&tid[i], NULL, thread_func, &tsc[i]);
		if (ret)
			die("Fail to create thread");
	}

	for (i = 0; i < nr_threads; i++) {
		ret = pthread_join(tid[i], NULL);
		if (ret)
			die("Fail to join");
	}

	total = 0;
	for (i = 0; i < nr_threads; i++)
		total += tsc[i];

	printf("\033[31mThreads: %2d Avg Cycles: %10lu \033[0m\n", nr_threads, total / nr_threads / NR_PAGES);
}

int main(void)
{
	setbuf(stdout, NULL);

	//test_pgfault_threads(1);
	//test_pgfault_threads(2);
	test_pgfault_threads(1);
	test_pgfault_threads(2);
	test_pgfault_threads(4);
	test_pgfault_threads(6);
	test_pgfault_threads(8);

	return 0;
}
