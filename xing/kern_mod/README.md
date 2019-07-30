# Design

## Page Advance (pgadvance)

Let's keep this kernel module simple. It's sole purpose is do to page advance,
nothing more, nothing less.


## Async Crossing

This module might be doing more than its name suggests.
Mostly because it already planted a small struct inside each `task_struct`.
It comes in handy when we want to do some per-application tuning.

### Skip LRU Insertion During Page Fault
Search `aci` at `mm/memory.c`.
And `usr_lib/test_disable_LRU.c`.
The trick is, we add a special flag to ACI framework.
