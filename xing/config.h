#ifndef _SHARED_CONFIG_H_
#define _SHARED_CONFIG_H_

/*
 * This determines if nested pgfault within libpoll
 * is checked. If you are sure libpoll won't cause
 * any pgfault, then disable this. Otherwise enable.
 *
 * This config affect both kernel and user code.
 */
//#define CONFIG_ASYNCX_CHECK_NESTED

#endif /* _SHARED_CONFIG_H_ */
