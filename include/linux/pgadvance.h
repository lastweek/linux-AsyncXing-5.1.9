/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _LINUX_PGADVANCE_H_
#define _LINUX_PGADVANCE_H_

struct pgadvance_callbacks {
	struct page *(*alloc_zero_page)(void);
	struct page *(*alloc_normal_page)(void);
};

extern struct pgadvance_callbacks pcb_live;

/* Public APIs for kernel module */
int register_pgadvance_callbacks(struct pgadvance_callbacks *pcb);
int unregister_pgadvance_callbacks(void);

#endif /* _LINUX_PGADVANCE_H_ */
