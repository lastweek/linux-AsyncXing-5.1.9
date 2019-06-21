/*
 * Copyright (c) 2019 Yizhou Shan. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/kprobes.h>
#include "xing.h"

static void __used dump_aci(struct async_crossing_info *aci)
{
	pr_info("Dump ACI:");
	pr_info("  jmp_user_address:     %#lx\n", aci->jmp_user_address);
	pr_info("  jmp_user_stack:       %#lx\n", aci->jmp_user_stack);
	pr_info("  shared_pages:         %#lx\n", aci->shared_pages);
	pr_info("  kva_shared_pages:     %p\n", aci->kva_shared_pages);
}

static void handle_delegate(struct asyncx_delegate_info *adi)
{
	struct task_struct *fault_tsk;
	struct async_crossing_info *aci;
	struct shared_page_meta *user_page;

	fault_tsk = adi->tsk;
	aci = fault_tsk->aci;
	user_page = aci->kva_shared_pages;

	/* Notify user its done */
	user_page->flags |= ASYNCX_PGFAULT_DONE;

	/* "Free" this slot */
	adi->flags = 0;
}

/*
 * XXX: each cpu has 1 fixed slot won't work.
 * because the faulting thread already return to
 * userspace, it can be re-scheduled.
 * Another pgfault might happen and override.
 *
 * Thus each cpu must have multiple slots
 *
 * We need to have a per-cpu array.
 */
#define NR_ADI_ENTRIES 8
struct asyncx_delegate_info adi_array[NR_ADI_ENTRIES];

static int worker_thread_func(void *_unused)
{
	pr_info("Delegation thread runs on CPU %d\n", smp_processor_id());
	while (1) {
		struct asyncx_delegate_info *adi;

		adi = &adi_array[0];
		if (likely(adi->flags))
			handle_delegate(adi);

		if (kthread_should_stop())
			break;
	}
	pr_info("Delegation thread exit CPU %d\n", smp_processor_id());
	return 0;
}

/*
 * Pass information to the remote fault handling thread
 */
static inline void delegate(struct task_struct *tsk,
			    unsigned long address)
{
	struct asyncx_delegate_info adi;

	adi.tsk = tsk;
	adi.address = address;
	adi.flags = 1;
	memcpy(&adi_array[0], &adi, sizeof(adi));
}

/*
 * @regs: user registers upon fault
 * @address: the faulting virtual address
 *
 * This function was mainly used for testing during early stage.
 */
__used
static void cb_post_pgfault(struct pt_regs *regs, unsigned long address)
{
	struct async_crossing_info *aci;
	struct shared_page_meta *user_page;
	struct pt_regs *user_page_regs;

	/* Check if registered */
	aci = current->aci;
	if (unlikely(!aci))
		return;
	user_page = (void *)aci->shared_pages;
	user_page_regs = &user_page->regs;

	/* We don't do nested handling */
	if (unlikely(user_page->flags & ASYNCX_INTERCEPTED))
		return;

	delegate(current, address);

	/* Save orignal fault context */
	user_page->flags = ASYNCX_INTERCEPTED;
	memcpy(user_page_regs, regs, sizeof(struct pt_regs));

	/* Replace with user register IP and SP */
	regs->ip = aci->jmp_user_address;
	regs->sp = aci->jmp_user_stack;
}

/*
 * Return 1 if intercept, return 0 if not.
 */
static int cb_intercept_do_page_fault(struct vm_area_struct *vma,
				      unsigned long address, unsigned int flags)
{
	pr_info("%s(): %d-%s adddr: %#lx\n",
		__func__, current->pid, current->comm, address);
	return 0;
}

static int handle_asyncx_set(struct async_crossing_info __user * uinfo)
{
	struct async_crossing_info *kinfo;
	struct page *page;
	int ret;

	kinfo = kmalloc(sizeof(*kinfo), GFP_KERNEL);
	if (!kinfo)
		return -ENOMEM;

	if (copy_from_user(kinfo, uinfo, sizeof(struct async_crossing_info))) {
		kfree(kinfo);
		return -EFAULT;
	}

	ret = get_user_pages(kinfo->shared_pages, 1, 0, &page, NULL);
	kinfo->p_shared_pages = page;
	kinfo->kva_shared_pages = page_to_virt(page);

	current->aci = kinfo;
	dump_aci(kinfo);
	return 0;
}

static int handle_asyncx_unset(struct async_crossing_info __user * uinfo)
{
	struct async_crossing_info kinfo = { };

	if (copy_from_user(&kinfo, uinfo, sizeof(struct async_crossing_info)))
		return -EFAULT;

	if (current->aci) {
		kfree(current->aci);
		current->aci = NULL;
	}
	return 0;
}

static int handle_asyncx_get(struct async_crossing_info __user * uinfo)
{
	struct async_crossing_info *aci;

	aci = current->aci;
	if (!aci)
		return -ENODEV;
	if (copy_to_user(uinfo, aci, sizeof(*aci)))
		return -EFAULT;
	return 0;
}

static int cb_syscall(int cmd, struct async_crossing_info __user * uinfo)
{
	int ret = 0;

	switch (cmd) {
	case ASYNCX_SET:
		ret = handle_asyncx_set(uinfo);
		break;
	case ASYNCX_UNSET:
		ret = handle_asyncx_unset(uinfo);
		break;
	case ASYNCX_GET:
		ret = handle_asyncx_get(uinfo);
		break;
	default:
		ret = -EINVAL;
	}
	return ret;
}

static struct asyncx_callbacks cb = {
	.syscall			= cb_syscall,
	.intercept_do_page_fault	= cb_intercept_do_page_fault,
	.post_pgfault_callback		= default_dummy_asyncx_post_pgfault,
};

struct task_struct *worker_thread;

int init_asyncx_thread(void)
{
	worker_thread = kthread_run(worker_thread_func, NULL, "async_worker");
	if (IS_ERR(worker_thread))
		return PTR_ERR(worker_thread);
	return 0;
}

void exit_asyncx_thread(void)
{
	if (!IS_ERR_OR_NULL(worker_thread))
		kthread_stop(worker_thread);
}

static int core_init(void)
{
	int ret;

	ret = register_asyncx_callbacks(&cb);
	if (ret) {
		pr_err("Fail to register asyncx callbacks.");
		return ret;
	}

	ret = init_asyncx_thread();
	if (ret) {
		pr_err("Fail to init delegate thread.");
		unregister_asyncx_callbacks(&cb);
		return ret;
	}

	return 0;
}

static void core_exit(void)
{
	unregister_asyncx_callbacks(&cb);
	exit_asyncx_thread();
}

module_init(core_init);
module_exit(core_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Yizhou Shan");
