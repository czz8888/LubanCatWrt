// SPDX-License-Identifier: GPL-2.0
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

#include <fdtdec.h>
#include <linux/libfdt.h>
#include <fdt_support.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <remoteproc.h>
#include <sunxi_rproc.h>
#include "sunxi_rproc_internal.h"
#include "sunxi_boot_reason.h"
#include "sunxi_rproc_boot_reason_dev.h"

// xxx = <N>;
static inline int fdt_getprop_u32(const void *fdt, int off, const char *prop, uint32_t *pval)
{
	int len;
	const fdt32_t *val = fdt_getprop(fdt, off, prop, &len);

	if (val == NULL || len != sizeof(*val))
		return -1;

	if (pval)
		*pval = fdt32_to_cpu(*val);
	return 0;
}

int sunxi_rproc_boot_reason_resource_get(struct sunxi_rproc_privdata *priv, const void *fdt, int node)
{
	struct sunxi_rproc_boot_reason_rsc *rsc = &priv->boot_reason_rsc;
	int ret;
	struct fdt_resource reg_res;
	const char *str, *status;
	int subnode;
	uint32_t num;

	SUNXI_RPROC_TRACE("priv: %p", priv);

	str = "boot_reason";
	subnode = fdt_subnode_offset(fdt, node, str);
	if (!subnode) {
		dev_err(priv->dev, "can't find subnode: %s\n", str);
		return -EINVAL;
	}

	status = (const char *)fdt_getprop(fdt, subnode, "status", NULL);
	if (!status || strcmp(status, "okay")) {
		dev_err(priv->dev, "%s node disabled", str);
		return -EINVAL;
	}
	str = "data_idx";
	ret = fdt_getprop_u32(fdt, subnode, str, &num);
	if (ret) {
		dev_err(priv->dev, "can't find u32: %s\n", str);
		return -EINVAL;
	}
	rsc->data_idx = num;

	str = "rtc_reg";
	ret = fdt_get_named_resource(fdt, node, "reg", "reg-names", str, &reg_res);
	if (ret) {
		dev_err(priv->dev, "can't find reg: %s\n", str);
		return -EINVAL;
	}

	rsc->rtc_base = map_physmem(reg_res.start, fdt_resource_size(&reg_res), MAP_NOCACHE);
	return 0;
}

int sunxi_rproc_boot_reason_update(void __iomem *rtc_base, unsigned int data_idx)
{
	enum boot_reason_t reason;

	SUNXI_RPROC_TRACE("rtc_base: %lx, data_idx: %u", (unsigned long)rtc_base, data_idx);

	if (!rtc_base)
		return -EINVAL;

	reason = get_soc_boot_reason(rtc_base);
	if (BOOT_REASON_SYS_COLD_RST == reason) {
		/* distinguish between reboot and cold start */
		reason = get_boot_reason(rtc_base, (int)data_idx);
		if (reason != BOOT_REASON_USER_STOP) {
			/* not reboot, restore reason */
			reason = BOOT_REASON_SYS_COLD_RST;
		}
	} else if (BOOT_REASON_INVALID_RST == reason) {
		/* may be prev has already been processed */
		reason = get_boot_reason(rtc_base, (int)data_idx);
	}
	printf("boot reason: %s\n", boot_reason_str(reason));
	set_boot_reason(rtc_base, (int)data_idx, reason);
	return 0;
}
