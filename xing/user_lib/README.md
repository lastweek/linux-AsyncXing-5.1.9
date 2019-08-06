# About measure k2u u2k xing latency

We have another repo just to measure the latency
of crossing. Here, we are measuring the overhead of
	crossing + saving registers
    and
        crossing + restoring registers

Inside kernel, the TSC sample points are both inside
`do_page_fault()`.

## Code

User-level, measure.c:

POST or PRE flag, only one can be enabled:
```c
	aci.flags = FLAG_DISABLE_PGFAULT_INTERCEPT |
		    FLAG_ENABLE_POST_FAULT;
or
	aci.flags = FLAG_DISABLE_PGFAULT_INTERCEPT |
		    FLAG_ENABLE_PRE_FAULT;
```

Kernel-level, `kern_mod/asyncx/core.c`:
```
static void cb_post_page_fault(struct pt_regs *regs)()
static void cb_pre_page_fault(struct pt_regs *regs)()
```

## Eval

On a Xeon E5 v3 2.4 GHz machine, we have
	U2K: 780 cycles
	K2U: 470 cycles
