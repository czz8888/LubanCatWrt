/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_riscv.h
 *
 * Copyright (c) 2007-2025 Allwinnertech Co., Ltd.
 * Author: shihongfu <shihongfu@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 *
 */

#ifndef __SUNXI_RPROC_RISCV_H__
#define __SUNXI_RPROC_RISCV_H__

#include "sunxi_rproc_internal.h"

extern struct sunxi_rproc_driver_data e907_driver_data;
#define SUNXI_RPROC_RISCV_IDS	\
	{.compatible = "allwinner,e907-rproc", .data = (ulong)&e907_driver_data }

/*
 * RISCV_CFG Register define
 */
#define RISCV_VER_REG               (0x0000) /* RISCV Version Register */
#define RISCV_RF1P_CFG_REG          (0x0010) /* RISCV Control Register0 */
#define RISCV_TS_TMODE_SEL_REG      (0x0040) /* RISCV TEST MODE SELETE Register */
#define RISCV_STA_ADD_REG           (0x0204) /* RISCV STAT Register */
#define RISCV_WAKEUP_EN_REG         (0x0220) /* RISCV WakeUp Enable Register */
#define RISCV_WAKEUP_MASK0_REG      (0x0224) /* RISCV WakeUp Mask0 Register */
#define RISCV_WAKEUP_MASK1_REG      (0x0228) /* RISCV WakeUp Mask1 Register */
#define RISCV_WAKEUP_MASK2_REG      (0x022C) /* RISCV WakeUp Mask2 Register */
#define RISCV_WAKEUP_MASK3_REG      (0x0230) /* RISCV WakeUp Mask3 Register */
#define RISCV_WAKEUP_MASK4_REG      (0x0234) /* RISCV WakeUp Mask4 Register */
#define RISCV_WORK_MODE_REG         (0x0248) /* RISCV Worke Mode Register */

/*
 * AXI monitor related
 */
#define AXI_MONITOR_TAKEN_OVER_REG          (0x0000)	/* AXI_MONITOR_TAKEN_OVER_REG */
#define AXI_MONITOR_TAKEN_OVER_ENABLE       (1 << 0)	/* AXI_MONITOR_TAKEN OVER ENABLE VALUE */
#define AXI_MONITOR_TAKEN_TIMEOUT_REG       (0x0020)	/* AXI_MONITOR_TAKEN_TIMEOUT_REG */
#define AXI_MONITOR_TAKEN_OVER_TIMEOUT_TIME (0x0040)	/* AXI_MONITOR_TAKEN OVER TIMEOUT VALUE */

#endif /* __SUNXI_RPROC_RISCV_H__ */
