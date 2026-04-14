/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef __SUNXI_BOOT_REASON_H__
#define __SUNXI_BOOT_REASON_H__

#include "sunxi_boot_reason_def.h"

enum boot_reason_t get_boot_reason(void __iomem *reg, int data_idx);
int set_boot_reason(void __iomem *reg, int data_idx, enum boot_reason_t reason);
void set_boot_reason_force(void __iomem *reg, int data_idx, enum boot_reason_t reason);
enum boot_reason_t get_soc_boot_reason(void __iomem *reg);

#endif /* __SUNXI_BOOT_REASON_H__ */
