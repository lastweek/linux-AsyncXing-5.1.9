#define _GNU_SOURCE

#include "includeme.h"
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>

static __attribute__((always_inline)) inline unsigned long rdtsc(void)
{
	unsigned long low, high;
	asm volatile("rdtscp" : "=a" (low), "=d" (high));
	return ((low) | (high) << 32);
}

int main(int argc, char **argv)
{
	int cpu, node;
	int fd;
	void *foo;
	long nr_size, i;
	struct timeval ts, te, result;
	struct stat st;
	size_t file_size, mmap_size;
	int NR_PAGES;
	unsigned long s_tsc, e_tsc;

	if (argc != 2)
		die("Usage: ./a.out file_to_open");

	pin_cpu(23);
	getcpu(&cpu, &node);
	printf("Runs on cpu: %d, node: %d\n", cpu, node);

	/* Open and get file size */
	fd = open(argv[1], O_RDWR);
	if (fd < 0) {
		perror("Reason: ");
		die("Fail to open file: %s", argv[1]);
	}
	fstat(fd, &st);
	file_size = st.st_size;
	NR_PAGES = file_size / PAGE_SIZE;
	mmap_size = NR_PAGES * PAGE_SIZE;
	printf("Using file: %s file_size: %zu B, nr_pages: %d\n", argv[1], file_size, NR_PAGES);

	foo = mmap(NULL, mmap_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
	if (foo == MAP_FAILED) {
		perror("Fail to mmap");
		exit(-1);
	}

	gettimeofday(&ts, NULL);
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
	gettimeofday(&te, NULL);
	timeval_sub(&result, &te, &ts);

	printf(" Runtime: %ld.%06ld s\n",
		result.tv_sec, result.tv_usec);
	printf(" NR_PAGES: %d\n", NR_PAGES);
	printf(" per_page_ns: %lu\n",
		(result.tv_sec * 1000000000 + result.tv_usec * 1000) / NR_PAGES);

	printf(" Total cycles: %lu Avg : %lu (cycles)\n",
		e_tsc - s_tsc, (e_tsc - s_tsc) / NR_PAGES);
	return 0;
}
