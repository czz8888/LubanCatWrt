/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * Configuration settings for the Allwinner A10-evb board.
 */

#ifndef _SUNXI_TIMER_H_
#define _SUNXI_TIMER_H_

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <asm/arch/watchdog.h>

/* General purpose timer */
struct sunxi_timer {
	volatile u32 ctl;
	volatile u32 inter;
	volatile u32 val;
	u8 res[4];
};

/* TODO: error !!!*/
struct sunxi_timer_reg {
	volatile u32 tirqen;		/* 0x00 */
	volatile u32 tirqsta;	/* 0x04 */
	uint     res1[2];
	struct sunxi_timer timer[2];		/* We have 2 timers */
	uint  	 res2[0x70/4];			/* 0x70 */
	struct sunxi_wdog wdog[1];		/* 0xa0 */
};

struct timer_list {
	unsigned int expires;
	void (*function)(void *data);
	unsigned long data;
	int   timer_num;
};

extern int  timer_init(void);

extern void timer_exit(void);

extern void watchdog_disable(void);

extern void watchdog_enable(void);

extern void init_timer(struct timer_list *timer);

extern void add_timer(struct timer_list *timer);

extern void del_timer(struct timer_list *timer);

extern void __usdelay(unsigned long usec);

extern void __msdelay(unsigned long msec);
extern ulong get_timer_masked(void);

#endif /* __ASSEMBLY__ */

#endif
