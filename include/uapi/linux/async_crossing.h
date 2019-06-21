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

/*
 * The kernel and user shared page.
 */
struct shared_page_meta {
	unsigned long flags;
	struct pt_regs regs;
};

#endif /* _LINUX_UAPI_ASYNC_CROSSING_H_ */
