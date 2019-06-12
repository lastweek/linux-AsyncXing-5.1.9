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
#include <linux/unistd.h>
#include <assert.h>
#include <sched.h>

static inline void die(const char * str, ...)
{
	va_list args;
	va_start(args, str);
	vfprintf(stderr, str, args);
	fputc('\n', stderr);
	exit(1);
}

static int pin_cpu(int cpu_id)
{
	int ret;

	cpu_set_t cpu_set;
	CPU_ZERO(&cpu_set);
	CPU_SET(cpu_id, &cpu_set);

	ret = sched_setaffinity(0, sizeof(cpu_set), &cpu_set);
	return ret;
}

static void getcpu(int *cpu, int *node)
{
	int ret;
	ret = syscall(SYS_getcpu, cpu, node, NULL);
}

/*
 * Check "arch/x86/include/generated/uapi/asm/unistd_64.h"
 */
static inline int async_crossing(void)
{
	int ret;

#ifndef __NR_async_crossing
# warning "not an async_crossing equipped kernel"
# define __NR_async_crossing	428
#endif

	ret = syscall(__NR_async_crossing);
	return ret;
}

int main(void)
{
	int i, cpu, node;

	getcpu(&cpu, &node);
	printf("Running on cpu: %d, node: %d\n", cpu, node);

	return 0;
}
