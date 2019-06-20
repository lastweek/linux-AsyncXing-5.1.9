/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef _ASYNCX_H_H_
#define _ASYNCX_H_H_

#include "../../include/uapi/linux/async_crossing.h"
#include <asm/ptrace.h>

static inline void save_registers(struct pt_regs *r)
{
	asm volatile (
		"____save:                      \n\t"
		"	movq	%%r15, %0	\n\t"
		"	movq	%%r14, %1	\n\t"
		"	movq	%%r13, %2	\n\t"
		"	movq	%%r12, %3	\n\t"
		"	movq	%%r11, %4	\n\t"
		"	movq	%%r10, %5	\n\t"
		"	movq	%%r9,  %6	\n\t"
		"	movq	%%r8,  %7	\n\t"
		"	movq	%%rax, %8	\n\t"
		"	movq	%%rbx, %9	\n\t"
		"	movq	%%rcx, %10	\n\t"
		"	movq	%%rdx, %11	\n\t"
		"	movq	%%rsi, %12	\n\t"
		"	movq	%%rdi, %13	\n\t"
		"____save_end:                  \n\t"
		: "=m" (r->r15),
		  "=m" (r->r14),
		  "=m" (r->r13),
		  "=m" (r->r12),
		  "=m" (r->r11),
		  "=m" (r->r10),
		  "=m" (r->r9),
		  "=m" (r->r8),
		  "=m" (r->rax),
		  "=m" (r->rbx),
		  "=m" (r->rcx),
		  "=m" (r->rdx),
		  "=m" (r->rsi),
		  "=m" (r->rdi)
		:
		: "memory"
	);
}

static void restore_registers(struct pt_regs *_r)
{
	struct pt_regs *r;
	asm volatile (
		"__restore_prep:"
		"movq %1, %0\n\t"
		: "=S"(r)
		: "a"(_r)
	);
	asm volatile (
		"__restore:                   \n\t"
		"	movq	%0,  %%r15    \n\t"
		"	movq	%1,  %%r14    \n\t"
		"	movq	%2,  %%r13    \n\t"
		"	movq	%3,  %%r12    \n\t"
		"	movq	%4,  %%r11    \n\t"
		"	movq	%5,  %%r10    \n\t"
		"	movq	%6,  %%r9     \n\t"
		"	movq	%7,  %%r8     \n\t"
		"	movq	%8,  %%rax    \n\t"
		"	movq	%9,  %%rbx    \n\t"
		"	movq	%10, %%rcx    \n\t"
		"	movq	%11, %%rdx    \n\t"
		"	movq	%13, %%rdi    \n\t"
		"	movq	%14, %%rsp    \n\t"
		"	movq	%15, %%rbp    \n\t"
		"	movq	%12, %%rsi    \n\t"
		:
		: "m" (r->r15),
		  "m" (r->r14),
		  "m" (r->r13),
		  "m" (r->r12),
		  "m" (r->r11),
		  "m" (r->r10),
		  "m" (r->r9),
		  "m" (r->r8),
		  "m" (r->rax),
		  "m" (r->rbx),
		  "m" (r->rcx),
		  "m" (r->rdx),
		  "m" (r->rsi),
		  "m" (r->rdi),
		  "m" (r->rsp),
		  "m" (r->rbp)
		: "memory"
	);
}

#endif /* _ASYNCX_H_H_ */
