/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 */

#ifndef _SUNXI_CLOCK_H
#define _SUNXI_CLOCK_H

/* clock control module regs definition */
#if defined(CONFIG_MACH_SUN252IW1)
#include <asm/arch/plat-sun252iw1p1/clock_sun252iw1.h>
#endif

struct core_pll_freq_tbl {
    int FactorN;
    int FactorK;
    int FactorM;
    int FactorP;
    int pading;
};

#ifndef __ASSEMBLY__
int clock_init(void);
void clock_init_safe(void);
void clock_init_sec(void);
void clock_init_uart(void);

#endif

#endif /* _SUNXI_CLOCK_H */
