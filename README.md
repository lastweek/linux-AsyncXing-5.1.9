# Asynchronous Kernel/User Crossing

Author: Yizhou Shan <ys@purdue.edu>

Some cool asynchronous kernel/user crossing stuff I'm doing.
Still WIP.

If you are interested, check out those locations:

- `xing/kern_mod`: kernel side module
	- NOTE: the reason to extract code and have a module
           is to avoid compiling kernel every time a line
	   is changed. Yes, an extra call is introduced,
	   but it greatly speedup development.
- `xing/user_lib`: user land library
- `arch/x86/kernel/async_crossing.c`: the new syscall
- `arch/x86/mm/fault.c`: the guy who got robbed
