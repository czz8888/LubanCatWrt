/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2014
 * Chen-Yu Tsai <wens@csie.org>
 *
 * Watchdog register definitions
 */

#ifndef _SUNXI_WATCHDOG_H_
#define _SUNXI_WATCHDOG_H_

#define WDT_CFG_KEY 0x16AA
#define WDT_CFG_SYS_RESTART (0x01 << 0)
#define WDT_CTRL_RESTART	(0x1 << 0)
#define WDT_CTRL_KEY		(0x0a57 << 1)

#define WDT_CFG_RESET		(0x1)
#define WDT_MODE_EN		(0x1)

struct sunxi_wdog {
	volatile u32 irq_en;		/* 0x00 */
	volatile u32 irq_sta;		/* 0x04 */
	volatile u32 res1[2];
	volatile u32 ctl;		/* 0x10 */
	volatile u32 cfg;		/* 0x14 */
	volatile u32 mode;		/* 0x18 */
	u32 res2;
};
#endif /* _SUNXI_WATCHDOG_H_ */
