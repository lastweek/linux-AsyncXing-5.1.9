# Notes


1. End of `do_page_fault()` to userspace

It consists of two parts:
- `entry_64.S`: restore registers, switch cr3 and gs
- kernel to user space crossing

Mechanism: I'm using the `post_page_fault` callback, which
will be called at the very end of `do_page_fault()`. The callback,
which is implemented at `kern_mod/core.c` will save the current tsc
into _user_ stack. At userspace, we save the tsc right after the
the pgfault instruction.

The numbers got from Xeon E5 v3 is around 475 cycles.

We measured the cost of kernel to user crossing, which is [~270 cycles](https://github.com/lastweek/linux-xperf-4.19.44).

That means.. restoring and switch cr3 etc only costs around 205 cycles. Is that so?


HOWTO RUN:
- `kern_mod/core.c`: make sure you registered the `cb_post_page_fault()`.
- `user_lib/core.c`: use the modified measureing function.
