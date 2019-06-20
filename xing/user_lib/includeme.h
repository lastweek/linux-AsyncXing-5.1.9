/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
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
#include <linux/unistd.h>
#include <assert.h>

#define BUG_ON(cond)	assert(!(cond))
#define BUG()		BUG_ON(1)

#define PAGE_SIZE 4096
#define PAGE_MASK (~(PAGE_SIZE - 1))

/*
 * This looks more complex than it should be. But we need to
 * get the type for the ~ right in round_down (it needs to be
 * as wide as the result!), and we want to evaluate the macro
 * arguments just once each.
 */
#define __round_mask(x, y)	((__typeof__(x))((y)-1))
#define round_up(x, y)		((((x)-1) | __round_mask(x, y))+1)
#define round_down(x, y)	((x) & ~__round_mask(x, y))

#define DIV_ROUND_UP(n,d)	(((n) + (d) - 1) / (d))

#define __ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define ALIGN(x, a)		__ALIGN_MASK(x, (typeof(x))(a) - 1)
#define PTR_ALIGN(p, a)		((typeof(p))ALIGN((unsigned long)(p), (a)))
#define IS_ALIGNED(x, a)	(((x) & ((typeof(x))(a) - 1)) == 0)

#define ARRAY_SIZE(x)		(sizeof(x) / sizeof((x)[0]))
#define __NR_CHECKPOINT	666

static inline void die(const char * str, ...)
{
	va_list args;
	va_start(args, str);
	vfprintf(stderr, str, args);
	fputc('\n', stderr);
	exit(1);
}

/*
 * result: diff
 * x: end time
 * y: start time
 */
static inline int timeval_sub(struct timeval *result, struct timeval *x,
			  struct timeval *y)
{
  /* Perform the carry for the later subtraction by updating y. */
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (x->tv_usec - y->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

  /* Compute the time remaining to wait.
     tv_usec is certainly positive. */
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  /* Return 1 if result is negative. */
  return x->tv_sec < y->tv_sec;
}

static inline struct timeval
timeval_add(struct timeval *a, struct timeval *b)
{
	struct timeval result;

	result.tv_sec = a->tv_sec + b->tv_sec;
	result.tv_usec = a->tv_usec + b->tv_usec;
	if (result.tv_usec >= 1000000) {
		result.tv_sec += 1;
		result.tv_usec -= 1000000;
	}
	return result;
}

static inline pid_t gettid(void)
{
	syscall(SYS_gettid);
}

static inline int pin_cpu(int cpu_id)
{
	int ret;

	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(cpu_id, &cpu_set);

	ret = sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
	return ret;
}

static inline void getcpu(int *cpu, int *node)
{
	int ret;
	ret = syscall(SYS_getcpu, cpu, node, NULL);
}
