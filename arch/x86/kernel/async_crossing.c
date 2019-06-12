/*
 * Copyright (c) 2019, Yizhou Shan <syzwhat@gmail.com>. All rights reserved.
 * Subject to GPLv2.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/user.h>
#include <linux/regset.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/async_crossing.h>

SYSCALL_DEFINE3(async_crossing, int, cmd,
		union async_crossing_attr __user *, uattr, unsigned int, size)
{
	return -ENOMEM;
}
