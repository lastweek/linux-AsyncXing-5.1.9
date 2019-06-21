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

#include "async.h"
#include "includeme.h"
#include "../../include/uapi/linux/async_crossing.h"

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
int __i=0;

#define NR_FAULTING_IP_TRAMPOLINE	128
unsigned long faulting_ip_trampoline[NR_FAULTING_IP_TRAMPOLINE];

static void __libpoll_entry(struct async_crossing_info *info)
{
	struct shared_page_meta *user_page;
	struct pt_regs *regs;
	int index;
	volatile unsigned long *tmp;

	index = 0;

	user_page = (void *)info->shared_pages;
	regs = &user_page->regs;
	faulting_ip_trampoline[index] = regs->rip;

	/* XXX: we need ACCESS_ONCE */
	tmp = &user_page->flags;
	while (1) {
		if (*tmp & ASYNCX_PGFAULT_DONE)
			break;
	}

#if 0
	printf("[%d] user_page: %#lx, faulting_ip: %#lx, done_flag: %#lx\n",
		__i++,
		user_page, faulting_ip_trampoline[index],
		user_page->flags);
#endif

	/*
	 * XXX:
	 * After restoring registers, there are only
	 * two possible ways to do the actual jump:
	 * 	- jmp 0x123(rip)
	 * 	- ret
	 *
	 * The first requires the destination is
	 * a compile-time known location.
	 *
	 * The second means we need to save the
	 * return address into original stack.
	 *
	 * For multiple threads, the first won't work.
	 */
	user_page->flags = 0;
	restore_registers(regs);
	asm volatile (
		"__ret:"
		"	jmp *%0\n\t"
		:
		: "m"(faulting_ip_trampoline[index])
		: "memory"
	);
	BUG();
}

static void libpoll_entry(void)
{
	__libpoll_entry(&aci);
}

int main(void)
{
	int i, cpu, node, ret;
	void *base_p;
	unsigned long jmp_user_address;
	void *shared_pages, *jmp_user_stack;
	int nr_shared_pages, nr_stack_pages;

	setbuf(stdout, NULL);

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

	aci.flags = 0;
	aci.jmp_user_address = jmp_user_address;
	aci.jmp_user_stack = (unsigned long)jmp_user_stack;
	aci.shared_pages = (unsigned long)shared_pages;
	aci.nr_shared_pages = nr_shared_pages;
	aci.nr_stack_pages = nr_stack_pages;

	printf("jmp_ip: %#lx jmp_sp: %#lx shared_page: %#lx\n",
		aci.jmp_user_address, aci.jmp_user_stack,
		aci.shared_pages);

	ret = syscall_async_crossing(ASYNCX_SET, &aci);
	if (ret) {
		perror("Fail to SET AsyncX");
		return ret;
	}

	memset(&aci, 0, sizeof(aci));
	ret = syscall_async_crossing(ASYNCX_GET, &aci);
	if (ret) {
		perror("Fail to GET AsyncX");
		return ret;
	}

	if (aci.jmp_user_address != jmp_user_address) {
		printf("** Error - Broken ASYNC_CROSSING syscall.\n"
		       "**      SET addr: %#lx\n"
		       "**      GET addr: %#lx\n",
		       jmp_user_address,
		       aci.jmp_user_address);

		unset_async_crossing(&aci);
		return -EINVAL;
	}

	/*
	 * Now test
	 */

	base_p = mmap(NULL, 4096 * 10, PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (base_p == MAP_FAILED)
		die("Fail to mmap()");

	for (i = 0; i < 10; i++) {
		int *bar, cut;

		bar = base_p + 4096 * i;
		*bar = 100 + i;
	}
	printf("We are done!\n");
	unset_async_crossing(&aci);
	exit(0);
	return 0;
}
