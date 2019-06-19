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

#include "../include/uapi/linux/async_crossing.h"

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

static void libpoll_entry(void)
{
	printf("We are here!\n");
	while (1)
		;
}

int main(void)
{
	int i, cpu, node, ret;
	struct async_crossing_info aci;
	void *base_p;
	unsigned long jmp_user_address;

	getcpu(&cpu, &node);
	printf("Running on cpu: %d, node: %d\n", cpu, node);

	jmp_user_address = (unsigned long)libpoll_entry;

	aci.flags = 0;
	aci.jmp_user_address = jmp_user_address;
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
		       "**      GET addr: %#lx\n"
		);
		return -EINVAL;
	}

	base_p = mmap(NULL, 4096 * 10, PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (base_p == MAP_FAILED)
		die("Fail to mmap()");

	for (i = 0; i < 10; i++) {
		int *bar, cut;

		bar = base_p + 4096 * i;
		printf("Touch %10d %#lx\n", i, bar);
		*bar = 100;
	}
	return 0;
}
