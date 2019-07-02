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
	asm volatile("rdtsc" : "=a" (low), "=d" (high));
	return ((low) | (high) << 32);
}

struct request {
	unsigned int flag;
	unsigned long unused;
} __attribute__((aligned(128)));

struct response {
	unsigned int flag;
	unsigned long unused;
} __attribute__((aligned(128)));

volatile int server_running = 0;

volatile struct request req __attribute__((aligned(128)));
volatile struct response resp __attribute__((aligned(128)));

int cpu_client = 20;
int cpu_server= 23;

int nr_delegations = 10000000;

int foo;

static inline void handle(void)
{
	foo++;
}

void *server_func(void *_unused)
{
	int cpu, node;
	pin_cpu(cpu_server);
	getcpu(&cpu, &node);
	printf("Server: CPU %d NODE: %d\n", cpu, node);
	server_running = 1;

	while (1) {
		/* Wait for request */
		while (!(req.flag ^ resp.flag))
			asm volatile("rep;nop": : :"memory");;

		handle();

		/* Send response */
		resp.flag = ~resp.flag;
	}
}

void client_func(void)
{
	int nr_runs;
	unsigned long s_tsc, e_tsc;

	nr_runs = nr_delegations;

	asm volatile("mfence": : :"memory");
	s_tsc = rdtsc();
	asm volatile("mfence": : :"memory");

	while (nr_runs--) {
		/* Send requests */
		req.unused = 100;
		req.flag = ~req.flag;

		/* Wait for response */
		while (req.flag ^ resp.flag)
			asm volatile("rep;nop": : :"memory");;
	}

	asm volatile("mfence": : :"memory");
	e_tsc = rdtsc();
	asm volatile("mfence": : :"memory");

	printf("Total cycles: %lu. Avg: %lu\n",
		e_tsc - s_tsc,
		(e_tsc - s_tsc) / nr_delegations);
}

int main(void)
{
	pthread_t pth;
	int ret;
	int cpu, node;

	pin_cpu(cpu_client);
	getcpu(&cpu, &node);

	printf("Req %#lx Resp %#lx;  nr_delegations: %d\n", &req, &resp, nr_delegations);
	printf("Client(): CPU %d NODE: %d\n", cpu, node);

	memset((void *)&req, 0, sizeof(req));
	memset((void *)&resp, 0, sizeof(resp));

	ret = pthread_create(&pth, NULL, server_func, NULL);
	if (ret)
		die("Fail to create server thread.");

	/* Wait server thread is live */
	while (!server_running)
		;

	client_func();
	client_func();

	//pthread_join(pth, NULL);
	printf("%d\n", foo);
	return 0;
}
