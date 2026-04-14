// SPDX-License-Identifier: GPL-2.0
/*
 * sunxi's boot reason driver
 * Public API for boot reason.
 *
 * Copyright (C) 2023 Allwinnertech - All Rights Reserved
 *
 * Author: shihongfu <shihongfu@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "sunxi_boot_reason_plat.h"

enum boot_reason_t get_boot_reason(void __iomem *reg, int data_idx)
{
	u32 val = readl(RTC_DATA_REG(reg, data_idx));
	br_trace("rtc boot reason reg read val: %x", (unsigned int)val);

	return reg_to_boot_reason(val);
}

void set_boot_reason_force(void __iomem *reg, int data_idx, enum boot_reason_t reason)
{
	u32 val = readl(RTC_DATA_REG(reg, data_idx));
	br_trace("rtc boot reason reg read val: %x", (unsigned int)val);

	val = boot_reason_to_reg(val, ROLE, reason);
	br_trace("rtc boot reason reg write val: %x", (unsigned int)val);
	writel(val, RTC_DATA_REG(reg, data_idx));
}

int set_boot_reason(void __iomem *reg, int data_idx, enum boot_reason_t reason)
{
	enum boot_reason_t last = get_boot_reason(reg, data_idx);

	br_trace("try update boot reason from %s to %s\n", boot_reason_str(last), boot_reason_str(reason));

	if (boot_reason_priority(last) > boot_reason_priority(reason))
		return -1;

	set_boot_reason_force(reg, data_idx, reason);
	return 0;
}

static inline void soc_boot_reason_enable(void __iomem *rtc_base, u32 mask)
{
	void __iomem *addr = RBT_REG_ADDR(rtc_base, RBT_RCD_CTRL_REG);
	u32 val = mask | RBT_MASK(RBT_CTRL_EN);
	writel(val, addr);
}

static inline void soc_boot_reason_clear(void __iomem *rtc_base)
{
	void __iomem *addr = RBT_REG_ADDR(rtc_base, RBT_RCD_STAT_CLR_REG);
	u32 val = RBT_STAT_SRC_CLR_ALL_MASK;
	writel(val, addr);
}

static inline void soc_boot_reason_update_internal(enum boot_reason_t *preason, enum boot_reason_t new)
{
	if (*preason != BOOT_REASON_INVALID_RST)
		br_print("covering from %s to %s\n", boot_reason_str(*preason), boot_reason_str(new));
	*preason = new;
}

enum boot_reason_t get_soc_boot_reason(void __iomem *rtc_base)
{
	u32 reg_val[2];
	enum boot_reason_t ret = BOOT_REASON_INVALID_RST;

	reg_val[0] = readl(RBT_REG_ADDR(rtc_base, RBT_RCD_CTRL_REG));
	br_trace("RBT_RCD_CTRL_REG: %x", (unsigned int)reg_val[0]);
	/* always on */
	soc_boot_reason_enable(rtc_base, 0);

	if (0 == (reg_val[0] & RBT_MASK(RBT_CTRL_EN))) {
		/* may be power on or reboot */
		return BOOT_REASON_SYS_COLD_RST;
	}
	if (0 != (reg_val[0] & RBT_CTRL_SRC_ALL_MASK))
		br_print("hw boot reason not all avail! %x\n", (unsigned int)reg_val[0]);

	reg_val[0] = readl(RBT_REG_ADDR(rtc_base, RBT_RCD_STAT_REG0));
	br_trace("RBT_RCD_STAT_REG0: %x", (unsigned int)reg_val[0]);

	reg_val[1] = readl(RBT_REG_ADDR(rtc_base, RBT_RCD_STAT_REG1));
	br_trace("RBT_RCD_STAT_REG1: %x", (unsigned int)reg_val[1]);

	soc_boot_reason_clear(rtc_base);

	/* fast path */
	if (reg_val[0] == 0 && reg_val[0] == 0) {
		/* may be prev has already been processed */
		return BOOT_REASON_INVALID_RST;
	}

	if (reg_val[0] & RBT_MASK(RBT_STAT0_SRC_TWD_TIG_TIM))
		soc_boot_reason_update_internal(&ret, BOOT_REASON_TWD_WDT_RST);
	if (reg_val[0] & RBT_MASK(RBT_STAT0_SRC_VDDSYSDET_TIG_TIM))
		soc_boot_reason_update_internal(&ret, BOOT_REASON_VDD_DET_RST);
	if (reg_val[0] & RBT_MASK(RBT_STAT0_SRC_VCCIODET_TIG_TIM))
		soc_boot_reason_update_internal(&ret, BOOT_REASON_VCC_DET_RST);
	if (reg_val[0] & RBT_MASK(RBT_STAT0_SRC_HRDRST_TIG_TIM))
		soc_boot_reason_update_internal(&ret, BOOT_REASON_SYS_HARD_RST);
	if (reg_val[1] & RBT_MASK(RBT_STAT1_SRC_MCUWDT_TIG_TIM))
		soc_boot_reason_update_internal(&ret, BOOT_REASON_MCU_WDT_RST);
	if (reg_val[1] & RBT_MASK(RBT_STAT1_SRC_SYSWDT_TIG_TIM))
		soc_boot_reason_update_internal(&ret, BOOT_REASON_SYS_WDT_RST);
	return ret;
}
