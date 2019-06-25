/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _LINUX_UAPI_ASYNC_CROSSING_H_
#define _LINUX_UAPI_ASYNC_CROSSING_H_

enum async_crossing_cmd {
	ASYNCX_SET,
	ASYNCX_UNSET,
	ASYNCX_GET,
};

struct async_crossing_info {
	unsigned long flags;
	unsigned long jmp_user_address;
	unsigned long jmp_user_stack;
	unsigned long shared_pages;
	unsigned long nr_shared_pages;
	unsigned long nr_stack_pages;

	/*
	 * The kernel virtual address of the
	 * shared_pages.
	 */
	struct page *p_shared_pages;
	void *kva_shared_pages;
};

/*
 * Shared poll page -> flags
 */
#define ASYNCX_PGFAULT_DONE	0x00000001
#define ASYNCX_INTERCEPTED	0x00000010

struct xing_padding {
	char x[0];
} __attribute__((__aligned__(64)));
#define XING_PADDING(name)	struct xing_padding name;


/*
 * The kernel and user shared page.
 */
struct shared_page_meta {
	/*
	 * This variable is shared between the handling CPU
	 * and the faulting CPU. So, make it cacheline aligned.
	 */
	unsigned long pgfault_done;
	XING_PADDING(_pad_1)

	/*
	 * This variable is only used by the faulting CPU
	 * to avoid nested pgfault interception.
	 */
	bool intercepted;
	XING_PADDING(_pad_2)
};

static inline void set_pgfault_done(struct shared_page_meta *p)
{
	p->pgfault_done = 1;
}

static inline void clear_pgfault_done(struct shared_page_meta *p)
{
	p->pgfault_done = 0;
}

static inline void set_intercepted(struct shared_page_meta *p)
{
	p->intercepted = true;
}

static inline void clear_intercepted(struct shared_page_meta *p)
{
	p->intercepted = false;
}

static inline bool check_intercepted(struct shared_page_meta *p)
{
	return p->intercepted;
}

#endif /* _LINUX_UAPI_ASYNC_CROSSING_H_ */
