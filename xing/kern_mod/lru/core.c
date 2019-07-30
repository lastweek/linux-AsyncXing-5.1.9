/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kprobes.h>
#include <linux/percpu.h>
#include <linux/gfp.h>
#include <linux/pagevec.h>

static int lru_microservice_func(void *_unused)
{
	while (1) {
		if (kthread_should_stop())
			break;
	}
}

static struct task_struct *lru_ms_task;

static int create_thread(void)
{
	struct task_struct *tsk;
	int cpu;

	tsk = kthread_create(lru_microservice_func, NULL, "kms_lru");
	if (IS_ERR(tsk))
		return PTR_ERR(tsk);

	cpu = 22;
	kthread_bind(tsk, cpu);
	wake_up_process(tsk);

	lru_ms_task = tsk;
	return 0;
}

static void destroy_thread(void)
{
	if (!lru_ms_task)
		return;

	kthread_stop(lru_ms_task);
}

static int lru_ms_init(void)
{
	int ret;

	ret = create_thread();
	if (ret) {
		pr_err("Fail to create threaad");
		return ret;
	}
	return 0;
}

static void lru_ms_exit(void)
{
	destroy_thread();
}

module_init(lru_ms_init);
module_exit(lru_ms_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
