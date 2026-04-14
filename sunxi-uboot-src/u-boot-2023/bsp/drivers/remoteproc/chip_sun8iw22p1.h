/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sunxi's rproc boot reason plat def
 * some chip definitions of boot reason for rproc.
 *
 * Copyright (C) 2023 Allwinnertech - All Rights Reserved
 *
 * Author: shihongfu <shihongfu@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __CHIP_SUN8IW22P1_H__
#define __CHIP_SUN8IW22P1_H__

#define RTC_DATA_REG(_base, _idx)		(((void __iomem *)_base) + 0x100 + 0x4 * _idx)

#define RBT_RCD_CTRL_REG_OFFSET			(0x240)
#define RBT_CTRL_EN_OFF				(0)
#define RBT_CTRL_EN_BITS			(0b1)
#define RBT_CTRL_CLK_SRC_SEL_OFF		(8)
#define RBT_CTRL_CLK_SRC_SEL_BITS		(0b111)
#define RBT_CTRL_SRC_TWD_OFF			(16)
#define RBT_CTRL_SRC_TWD_BITS			(0b1)
#define RBT_CTRL_SRC_VDDSYSDET_OFF		(17)
#define RBT_CTRL_SRC_VDDSYSDET_BITS		(0b1)
#define RBT_CTRL_SRC_VCCIODET_OFF		(18)
#define RBT_CTRL_SRC_VCCIODET_BITS		(0b1)
#define RBT_CTRL_SRC_HRDWDT_OFF			(19)
#define RBT_CTRL_SRC_HRDWDT_BITS		(0b1)
#define RBT_CTRL_SRC_SYSWDT_OFF			(20)
#define RBT_CTRL_SRC_SYSWDT_BITS		(0b1)
#define RBT_CTRL_SRC_MCUWDT_OFF			(21)
#define RBT_CTRL_SRC_MCUWDT_BITS		(0b1)

#define RBT_CTRL_SRC_MASK(_name)		RBT_MASK(CONTACT(RBT_CTRL_SRC_, _name))
#define RBT_CTRL_SRC_ALL_MASK			(0 \
							| RBT_CTRL_SRC_MASK(TWD)\
							| RBT_CTRL_SRC_MASK(VDDSYSDET) \
							| RBT_CTRL_SRC_MASK(VCCIODET) \
							| RBT_CTRL_SRC_MASK(HRDWDT) \
							| RBT_CTRL_SRC_MASK(SYSWDT) \
							| RBT_CTRL_SRC_MASK(MCUWDT) \
						)

#define RBT_RCD_STAT_REG0_OFFSET		(0x244)
#define RBT_STAT0_SRC_TWD_TIG_TIM_OFF		(0)
#define RBT_STAT0_SRC_TWD_TIG_TIM_BITS		(0b11111)
#define RBT_STAT0_SRC_VDDSYSDET_TIG_TIM_OFF	(8)
#define RBT_STAT0_SRC_VDDSYSDET_TIG_TIM_BITS	(0b11111)
#define RBT_STAT0_SRC_VCCIODET_TIG_TIM_OFF	(16)
#define RBT_STAT0_SRC_VCCIODET_TIG_TIM_BITS	(0b11111)
#define RBT_STAT0_SRC_HRDRST_TIG_TIM_OFF	(24)
#define RBT_STAT0_SRC_HRDRST_TIG_TIM_BITS	(0b11111)

#define RBT_RCD_STAT_REG1_OFFSET		(0x248)
#define RBT_STAT1_SRC_SYSWDT_TIG_TIM_OFF	(0)
#define RBT_STAT1_SRC_SYSWDT_TIG_TIM_BITS	(0b11111)
#define RBT_STAT1_SRC_MCUWDT_TIG_TIM_OFF	(8)
#define RBT_STAT1_SRC_MCUWDT_TIG_TIM_BITS	(0b11111)

#define RBT_RCD_STAT_CLR_REG_OFFSET		(0x258)

#define RBT_SRC_TWD_RCD_STAT_CLR_OFF		(0)
#define RBT_SRC_TWD_RCD_STAT_CLR_BITS		(0b1)
#define RBT_SRC_VDDSYSDET_RCD_STAT_CLR_OFF	(2)
#define RBT_SRC_VDDSYSDET_RCD_STAT_CLR_BITS	(0b1)
#define RBT_SRC_VCCIODET_RCD_STAT_CLR_OFF	(4)
#define RBT_SRC_VCCIODET_RCD_STAT_CLR_BITS	(0b1)
#define RBT_SRC_HRDWDT_RCD_STAT_CLR_OFF		(6)
#define RBT_SRC_HRDWDT_RCD_STAT_CLR_BITS	(0b1)
#define RBT_SRC_SYSWDT_RCD_STAT_CLR_OFF		(8)
#define RBT_SRC_SYSWDT_RCD_STAT_CLR_BITS	(0b1)
#define RBT_SRC_MCUWDT_RCD_STAT_CLR_OFF		(10)
#define RBT_SRC_MCUWDT_RCD_STAT_CLR_BITS	(0b1)
#define RBT_RCD_LST_TIM_CLR_OFF			(16)
#define RBT_RCD_LST_TIM_CLR_BITS		(0b1)
#define RBT_STAT_SRC_CLR_MASK(_name)		RBT_MASK(CONTACT(CONTACT(RBT_SRC_, _name), _RCD_STAT_CLR))
#define RBT_STAT_SRC_CLR_ALL_MASK		(0 \
							| RBT_STAT_SRC_CLR_MASK(TWD)\
							| RBT_STAT_SRC_CLR_MASK(VDDSYSDET) \
							| RBT_STAT_SRC_CLR_MASK(VCCIODET) \
							| RBT_STAT_SRC_CLR_MASK(HRDWDT) \
							| RBT_STAT_SRC_CLR_MASK(SYSWDT) \
							| RBT_STAT_SRC_CLR_MASK(MCUWDT) \
						)

#endif /* __CHIP_SUN8IW22P1_H__ */