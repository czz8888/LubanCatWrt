/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sunxi's rproc boot reason driver
 * Public API for rproc boot reason.
 *
 * Copyright (C) 2023 Allwinnertech - All Rights Reserved
 *
 * Author: shihongfu <shihongfu@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SUNXI_RPROC_BOOT_REASON_DEV_H__
#define __SUNXI_RPROC_BOOT_REASON_DEV_H__

int sunxi_rproc_boot_reason_resource_get(struct sunxi_rproc_privdata *priv, const void *fdt, int node);
int sunxi_rproc_boot_reason_update(void __iomem *rtc_base, unsigned int data_idx);

#endif /* __SUNXI_RPROC_BOOT_REASON_DEV_H__ */