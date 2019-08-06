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

static int syscall_async_crossing(int cmd, struct async_crossing_info *aci)
{
	int ret;

/* Check "arch/x86/include/generated/uapi/asm/unistd_64.h" */
#ifndef __NR_async_crossing
# define __NR_async_crossing	428
#endif
	ret = syscall(__NR_async_crossing, cmd, aci);
	return ret;
}

static void unset_async_crossing(struct async_crossing_info *aci)
{
	int ret;
	ret = syscall_async_crossing(ASYNCX_UNSET, aci);
	if (ret)
		perror("UNSET");
}

struct async_crossing_info aci;

static void __libpoll_entry(struct async_crossing_info *info)
{
	struct shared_page_meta *user_page;
	volatile unsigned long *done;

	user_page = (void *)info->shared_pages;
	done = &user_page->pgfault_done;
	while (1) {
		if (*done)
			break;
	}

	clear_pgfault_done(user_page);

#ifdef CONFIG_ASYNCX_CHECK_NESTED
	clear_intercepted(user_page);
#endif
}

/*
 * Upon entry:
 * 1) Stack Layout:
 *      | ..       |
 *      | ..       | <- Original RSP upon pgfault
 *      | fault_ip | <- Current RSP upon entry
 *      |          |
 *
 * 2) After pop_registers(), the stack layout
 *    will be just like above, so a simple ret
 *    will return back to the orignal fault_ip.
 * 3) The catch, is to minimize register usage.
 *    And whatever we are going to use, save them
 *    in push_registers();
 */
static void libpoll_entry(void)
{
	push_registers();

	__libpoll_entry(&aci);

	pop_registers();
}

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
	asm volatile("rdtsc" : "=a" (low), "=d" (high));
	return ((low) | (high) << 32);
}

#define NR_PAGES 1000000ULL

volatile int *bar;

static __attribute__((always_inline)) inline void
postfault_lat(void)
{
	void *foo;
	long nr_size, i;
	unsigned long total_diff = 0;
	unsigned long sp;
	unsigned long u_tsc, k_tsc;
	unsigned long *k_tsc_p;
	unsigned long cushion[10];

	nr_size = NR_PAGES * PAGE_SIZE;
	foo = mmap(NULL, nr_size, PROT_READ|PROT_WRITE,
		   MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (foo == MAP_FAILED)
		die("fail to malloc");

	sp = current_stack_pointer();
	k_tsc_p = (unsigned long *)(sp);
	printf("sp: %#lx\n", k_tsc_p);

	for (i = 0; i < NR_PAGES; i++) {
		bar = foo + PAGE_SIZE * i;

#if 0
		asm volatile("mfence": : :"memory");
		u_tsc = rdtsc();
#endif

		*bar = 100;

#if 1
		u_tsc = rdtsc();
		asm volatile("mfence": : :"memory");
#endif

		k_tsc = *k_tsc_p;

		printf("%d %lu\n", i, u_tsc - k_tsc);
		total_diff += u_tsc - k_tsc;

		//total_diff += k_tsc - u_tsc;
		//printf("%d %lu\n", i, k_tsc - u_tsc);
	}

	printf(" crossing total: %lu cycles, avg: %lu cycles\n",
		total_diff, total_diff/NR_PAGES);
}

int main(void)
{
	int i, cpu, node, ret;
	unsigned long jmp_user_address;
	void *shared_pages, *jmp_user_stack;
	int nr_shared_pages, nr_stack_pages;

	setbuf(stdout, NULL);

	pin_cpu(21);
	getcpu(&cpu, &node);
	printf("Running on cpu: %d, node: %d\n", cpu, node);

	/*
	 * Set the jmp address and then verify.
	 * Bail out if things went wrong.
	 */

	jmp_user_address = (unsigned long)libpoll_entry;

	nr_shared_pages = 1;
	nr_stack_pages = 1;

	shared_pages = mmap(NULL, PAGE_SIZE * (nr_shared_pages + nr_stack_pages),
			    PROT_READ | PROT_WRITE,
			    MAP_PRIVATE | MAP_ANONYMOUS | MAP_LOCKED | MAP_POPULATE, 0, 0);
	if (shared_pages == MAP_FAILED) {
		perror("Fail to create mlocked shared pages.");
		return -ENOMEM;
	}
	memset(shared_pages, 0, PAGE_SIZE * (nr_shared_pages + nr_stack_pages));

	jmp_user_stack = shared_pages + nr_shared_pages * PAGE_SIZE +
			 nr_stack_pages * PAGE_SIZE - 8;

	aci.jmp_user_address = jmp_user_address;
	aci.jmp_user_stack = (unsigned long)jmp_user_stack;
	aci.shared_pages = (unsigned long)shared_pages;
	aci.nr_shared_pages = nr_shared_pages;
	aci.nr_stack_pages = nr_stack_pages;

	printf("jmp_ip: %#lx jmp_sp: %#lx shared_page: %#lx\n",
		aci.jmp_user_address, aci.jmp_user_stack,
		aci.shared_pages);

	aci.flags = FLAG_DISABLE_PGFAULT_INTERCEPT |
		    FLAG_ENABLE_POST_FAULT;

	ret = syscall_async_crossing(ASYNCX_SET, &aci);
	if (ret) {
		perror("Fail to SET AsyncX");
		return ret;
	}

	postfault_lat();

	unset_async_crossing(&aci);
	return 0;
}
