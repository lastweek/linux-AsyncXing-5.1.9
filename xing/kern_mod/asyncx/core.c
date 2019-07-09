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
#include "../../config.h"

static void __used dump_aci(struct async_crossing_info *aci)
{
	pr_info("Dump ACI:");
	pr_info("  jmp_user_address:     %#lx\n", aci->jmp_user_address);
	pr_info("  jmp_user_stack:       %#lx\n", aci->jmp_user_stack);
	pr_info("  shared_pages:         %#lx\n", aci->shared_pages);
	pr_info("  kva_shared_pages:     %#lx\n", (unsigned long)aci->kva_shared_pages);
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

int cb_syscall(int cmd, struct async_crossing_info __user * uinfo)
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
/*
 * Bottom half of the pgfault handling on remote CPU.
 * Be careful on what's needed, always check where we
 * intercepted do_page_fault()!
 */
static void handle_intercept_delegate(struct asyncx_delegate_info *adi)
{
	struct task_struct *tsk;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct async_crossing_info *aci;
	struct shared_page_meta *user_page;
	vm_fault_t fault;
	unsigned long address = adi->address;

	tsk = adi->tsk;
	mm = tsk->mm;
	aci = tsk->aci;
	user_page = aci->kva_shared_pages;

	/*
	 * Do the dirty work
	 */

	down_read(&mm->mmap_sem);

	vma = find_vma(mm, address);
	if (unlikely(!vma)) {
		pr_err("Bad addr\n");
		goto err;
	}
	if (likely(vma->vm_start <= address))
		goto good_area;
	if (unlikely(!(vma->vm_flags & VM_GROWSDOWN))) {
		pr_err("Bad vma growsup\n");
		goto err;
	}
	if (unlikely(expand_stack(vma, address))) {
		pr_err("Bad expand stack\n");
		goto err;
	}

good_area:
	fault = handle_mm_fault(vma, address, adi->pgfault_flags);
	up_read(&mm->mmap_sem);

	/* Notify user */
	set_pgfault_done(user_page);

	/* XXX: "Free" this slot */
	adi->flags = 0;

	return;
err:
	up_read(&mm->mmap_sem);
	user_page->pgfault_done = 2;
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
struct asyncx_delegate_info adi_array[NR_ADI_ENTRIES] __cacheline_aligned;

static int worker_thread_func(void *_unused)
{
	struct asyncx_delegate_info *adi;
	adi = &adi_array[0];

	pr_info("Delegation thread runs on CPU %d Node %d\n",
		smp_processor_id(), numa_node_id());

	while (1) {
		if (likely(adi->flags))
			handle_intercept_delegate(adi);

		if (kthread_should_stop())
			break;
	}

	pr_info("Delegation thread exit CPU %d\n", smp_processor_id());
	return 0;
}

/*
 * Pass information to the remote fault handling thread
 */
static inline void
intercept_delegate(struct task_struct *tsk, struct vm_area_struct *vma,
		   unsigned long address, unsigned int pgfault_flags)
{
	struct asyncx_delegate_info *adi = &adi_array[0];

	adi->tsk = tsk;
	adi->address = address;
	adi->pgfault_flags = pgfault_flags;

	adi->flags = 1;
}

/*
 * Callback before any actual page fault handling.
 * We are in the faulting thread context.
 */
static enum intercept_result
cb_intercept_do_page_fault(struct pt_regs *regs, struct vm_area_struct *vma,
			   unsigned long address, unsigned int flags)
{
	struct async_crossing_info *aci;
	struct shared_page_meta *user_page;

	/* Check if registered */
	aci = current->aci;
	if (unlikely(!aci)) {
		return ASYNCX_PGFAULT_NOT_INTERCEPTED;
	}

	user_page = aci->kva_shared_pages;

#ifdef CONFIG_ASYNCX_CHECK_NESTED
	if (unlikely(check_intercepted(user_page))) {
		/*
		 * This came from the user libpoll code.
		 * But we cannot handle nested interception.
		 */
		pr_crit("%s(): nested pgfault\n", __func__);
		return ASYNCX_PGFAULT_NOT_INTERCEPTED;
	}
	set_intercepted(user_page);
#endif

	/* Delegate pgfault to remote CPU */
	intercept_delegate(current, vma, address, flags);

	/*
	 * Save the original faulting IP into the user stack.
	 * Hope.. we are not corrupting the stack.
	 */
	*(unsigned long *)(regs->sp - 8) = regs->ip;
	regs->sp -= 8;
	regs->ip = aci->jmp_user_address;

	return ASYNCX_PGFAULT_INTERCEPTED;
}

/*
 * Used to measure the kernel to user crossing overhead
 * Note that this is not a pure corssing latency, rather,
 * it includes some overhead to pop registers in entry_64.S
 */
__used
static void cb_measure_crossing_latency(struct pt_regs *regs)
{
	if (likely(current->aci))
		*(unsigned long *)(regs->sp) = rdtsc_ordered();
}

__used
static void cb_post_page_fault(struct pt_regs *regs, unsigned long address)
{
	if (likely(current->aci))
		*(unsigned long *)(regs->sp) = rdtsc_ordered();
}

static struct asyncx_callbacks cb = {
	.syscall			= cb_syscall,
	.intercept_do_page_fault	= cb_intercept_do_page_fault,

	/* DO NOT ENABLE MEASUREMENT */
	.post_pgfault_callback		= default_dummy_asyncx_post_pgfault,
	.measure_crossing_latency	= default_dummy_measure_crossing_latency,
};

/*
 * This dummy callback sets is mainly for testing.
 * It should NOT intercept any pgfault, rather, it only
 * provide the syscall interface. We use the tsk->aci
 * to do some other nasty hacking.
 */
__used
static struct asyncx_callbacks measure_cb = {
	.syscall			= cb_syscall,
	.intercept_do_page_fault	= default_dummy_intercept,

	/*
	 * Both the following callback can do measures
	 * the post_pgfault_callback() somehow should
	 * give us a more precise one.
	 */
	.post_pgfault_callback		= cb_post_page_fault,
	.measure_crossing_latency	= default_dummy_measure_crossing_latency,
};

struct task_struct *worker_thread;

int init_asyncx_thread(void)
{
	int cpu;

	/*
	 * NOTE! CPU matters a lot for delegation cost.
	 * Make sure the faulting CPU and the handling CPU
	 * are on the same socket, seperate physical cores will be plus!
	 */
	cpu = 19; 
	worker_thread = kthread_create(worker_thread_func, NULL, "async_wrk");
	if (IS_ERR(worker_thread))
		return PTR_ERR(worker_thread);

	kthread_bind(worker_thread, cpu);
	wake_up_process(worker_thread);
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
	//ret = register_asyncx_callbacks(&measure_cb);
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
