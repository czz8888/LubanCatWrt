/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_internal.h
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

#ifndef __SUNXI_RPROC_INTERNAL_H__
#define __SUNXI_RPROC_INTERNAL_H__

#include <linux/list.h>
#include <dm/device.h>
#include <clk.h>
#include <reset.h>

//#define RPROC_DEBUG

#ifdef RPROC_DEBUG
#define SUNXI_RPROC_DEBUG
#endif

#define SUNXI_RPROC_PRINTF(_cond, _title, fmt, arg...) \
	do { \
		if (_cond) \
			printf(_title "%s " fmt "\n", __func__, ##arg); \
	} while (0)

#ifdef SUNXI_RPROC_DEBUG
#define SUNXI_RPROC_TRACE(fmt, arg...) SUNXI_RPROC_PRINTF(1, "trace ", fmt, ##arg)
#else
#define SUNXI_RPROC_TRACE(fmt, arg...)
#endif

struct firmware {
	size_t size;
	const u8 *data;

	/* firmware loader private fields */
	void *priv;
};

#define DEFAULT_CORE_RATE	(480000000)
#define DEFAULT_AXI_RATE	(240000000)

#define DEFAULT_CORE_RATE	(480000000)
#define DEFAULT_AXI_RATE	(240000000)

enum rsc_handling_status {
	RSC_HANDLED	= 0,
	RSC_IGNORED	= 1,
};

#if IS_ENABLED(CONFIG_AW_RPROC_BOOT_REASON)
struct sunxi_rproc_boot_reason_rsc {
	void __iomem *rtc_base;
	unsigned int data_idx;
};
#endif

/**
 * enum rproc_state - remote processor states
 * @RPROC_OFFLINE:	device is powered off
 * @RPROC_RUNNING:	device is up and running
 * @RPROC_ATTACHED:	device has been booted by another entity and the core
 *			has attached to it
 * @RPROC_DETACHED:	device has been booted by another entity and waiting
 *			for the core to attach to it
 */
typedef enum aw_rproc_state {
	AW_RPROC_OFFLINE = 0,
	AW_RPROC_RUNNING = 1,
	AW_RPROC_DETACHED = 2,
	AW_RPROC_ATTACHED = 3,
} aw_rproc_state_t;

struct sunxi_rproc_privdata {
	// object
	struct udevice *dev;
	struct dm_rproc_uclass_pdata *uc_pdata;
	struct sunxi_rproc_driver_data *driver_data;
	bool running;
	aw_rproc_state_t state;

	// config from dts
	bool auto_boot;
	int mem_maps_cnt;
	struct sunxi_rproc_memory_mapping *mem_maps;
	u64 fw_mem;
	u64 fw_mem_size;
	// firmware
	struct firmware fw;
	struct resource_table *table_ptr;
	size_t table_sz;
#if IS_ENABLED(CONFIG_AW_RPROC_BOOT_REASON)
	// boot reason
	struct sunxi_rproc_boot_reason_rsc boot_reason_rsc;
#endif

	// ops
	int (*find_rsc_table)(const void *fw_addr, size_t fw_size, const void **table_ptr,
			      size_t *table_sz);
};

struct sunxi_rproc_riscv_cfg {
	void __iomem	*cfg_base;
#if IS_ENABLED(CONFIG_MACH_SUN8IW22)
	void __iomem	*axi_monitor_base;
#endif
	unsigned int core_rate;
	unsigned int axi_rate;
	struct clk input;
	struct clk core_clk;
	struct clk cfg_clk;
	struct clk ts_clk;
	struct clk axi_clk;
	struct clk axi_mon_clk;
	struct reset_ctl core_rst;
	struct reset_ctl sys_rst;
	struct reset_ctl cfg_rst;
	struct reset_ctl axi_mon_rst;

	const char *regulator_name;
	bool vf_check_result;
};

struct sunxi_rproc_driver_data {
	int (*find_rsc_table)(const void *fw_addr, size_t fw_size, const void **table_ptr,
			      size_t *table_sz);
	struct sunxi_rproc_riscv_cfg cfg;
};

struct vf_info{
	unsigned int dvfs;
	unsigned int voltage; /* mv */
	unsigned int core_rate_range;
};

int sunxi_rproc_start(struct udevice *dev);
void update_riscv_irq_tab(unsigned long elf_base, int idx);
int rproc_priv_parser_resource(struct udevice *dev);

#endif /* __SUNXI_RPROC_INTERNAL_H__ */
