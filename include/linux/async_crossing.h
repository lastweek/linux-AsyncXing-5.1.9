/*
 * Copyright (c) 2019, Yizhou Shan <syzwhat@gmail.com>. All rights reserved.
 * Subject to GPLv2.
 */
#ifndef _LINUX_ASYNC_CROSSING_H_
#define _LINUX_ASYNC_CROSSING_H_

#include <uapi/linux/async_crossing.h>

void setup_asyncx_jmp(struct pt_regs *regs, unsigned long address);

#endif /* _LINUX_ASYNC_CROSSING_H_ */
