// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/remoteproc/sunxi_rproc_user_resource.c
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

#include "sunxi_rproc_rsc_helper.h"
#include "sunxi_rproc_misc.h"
#include "sunxi_rproc_internal.h"
#include <errno.h>
#include <remoteproc.h>

#ifdef RPROC_DEBUG
#define USER_RESOURCE_DEBUG
#endif

#define USER_RESOURCE_PRINTF(_cond, _title, fmt, arg...) \
	do { \
		if (_cond) \
			printf(_title "%s " fmt "\n", __func__, ##arg); \
	} while (0)

#ifdef USER_RESOURCE_DEBUG
#define USER_RESOURCE_TRACE(fmt, arg...)	USER_RESOURCE_PRINTF(1, "trace ", fmt, ##arg)
#else
#define USER_RESOURCE_TRACE(fmt, arg...)
#endif

#define LOAD_SHARED_MEM_INFO		(0xfe)
#define LOAD_FROM_PARTITION		(0xff)

#define USER_RESOURCE_TYPE		(RSC_VENDOR_START + 2)

static inline int load_user_resource(struct rsc_node *rsc)
{
	int ret;
	struct fw_rsc_user_resource *info = rsc->rsc_info;

	USER_RESOURCE_TRACE("");

	switch (info->src_type) {
	case LOAD_FROM_PARTITION:
		ret = load_from_partition((char *)info->src_name, rsc->cache, info->len);
		pr_info("user_resource: load_from_partition(%s, %u) return %d\n",
		       info->src_name, (unsigned int)info->len, ret);
		flush_cache_wapper((unsigned long)rsc->cache, info->len);
#ifdef RPROC_DEBUG
		hex_dump_wapper((unsigned long)rsc->cache, info->len);
#endif
		return ret;
	case LOAD_SHARED_MEM_INFO:
		ret = load_shared_mem_info((char *)info->src_name, rsc->cache, info->len);
		pr_info("user_resource: load_shared_mem_info(%s, %u) return %d\n",
		       info->src_name, (unsigned int)info->len, ret);
		flush_cache_wapper((unsigned long)rsc->cache, info->len);
#ifdef RPROC_DEBUG
		hex_dump_wapper((unsigned long)rsc->cache, info->len);
#endif
		return ret;
	default:
		printf("src type unknown: %u\n", (unsigned int)info->src_type);
		return -EINVAL;
	}
}

static void show_rsc_user_resource(void *_info)
{
	struct fw_rsc_user_resource *info = _info;

	USER_RESOURCE_TRACE("");

	printf("user_resource: src_type: %u, da: %lx, len: %u, flags: 0x%02x, reserved[0-2]: %u %u %u, name: %s\n",
	       (unsigned int)info->src_type, (unsigned long)info->da,
	       (unsigned int)info->len, info->flags,
	       info->reserved[0], info->reserved[1], info->reserved[2],
	       info->src_name);
}

#include <dm/device_compat.h>
static int prepare_rsc_user_resource(struct udevice *dev, struct rsc_node *rsc)
{
	int ret, is_data_loaded;
	struct fw_rsc_user_resource *info = rsc->rsc_info;
	const struct dm_rproc_ops *ops;
	struct sunxi_rproc_privdata *aw_rproc;

	USER_RESOURCE_TRACE("amp user resource: %p", info);
#ifdef RPROC_DEBUG
	show_rsc_user_resource(info);
#endif

	aw_rproc = dev_get_priv(dev);
	if (aw_rproc->state == AW_RPROC_DETACHED) {
		/* We need to check flags only when the rproc has been booted on early stage */
		if (info->flags & AUR_FLAG_DATA_LOADED) {
			/* the data has been loaded on early stage(boot0 stage) */
			dev_info(dev, "the data of amp user resource('%s') has been loaded on early stage, skip load!\n",
				info->src_name);
			return 0;
		}

		dev_info(dev, "the data of amp user resource('%s') is not loaded on early stage, try to load!\n",
			info->src_name);
	}

	is_data_loaded = 0;
	ops = rproc_get_ops(dev);
	if (ops && ops->device_to_virt)
		rsc->cache = (void *)ops->device_to_virt(dev, info->da, info->len);
	else
		rsc->cache = (void *)info->da;
	if (!rsc->cache) {
		printf("%s get va failed!\n", __func__);
		ret = -ENOMEM;
		goto err_out;
	}

	ret = load_user_resource(rsc);
	if (ret < 0)
		goto err_out;

	is_data_loaded = 1;
	ret = 0;
err_out:

	if (is_data_loaded) {
		info->flags |= AUR_FLAG_DATA_LOADED;
	} else {
		info->flags &= ~AUR_FLAG_DATA_LOADED;
	}
	flush_cache_wapper((unsigned long)rsc->rsc_info, rsc->ops->rsc_size);

	rsc->cache = NULL;
	return ret;
}

const struct rsc_ops user_resource_ops = {
	.name = "user_resource",
	.type = USER_RESOURCE_TYPE,
	.rsc_size = sizeof(struct fw_rsc_user_resource),
	.show_rsc = show_rsc_user_resource,
	.prepare_rsc = prepare_rsc_user_resource,
};
