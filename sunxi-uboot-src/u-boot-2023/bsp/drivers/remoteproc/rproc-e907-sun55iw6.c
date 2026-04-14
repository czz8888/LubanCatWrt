// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/remoteproc/rproc-e907-sun55iw6.c
 *
 * Copyright (c) 2007-2025 Allwinnertech Co., Ltd.
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

#include "sunxi_rproc_internal.h"
#include "sunxi_rproc_riscv.h"
#include <asm/io.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <power-domain.h>
#include <power/regulator.h>
#include <sunxi_efuse_map.h>

extern ulong entry_addr;

#define SUN55IW6_DVFS_EFUSE_OFF (0x20)

#define SUN55IW6_DVFS_RV_VF0   (0x00)
#define SUN55IW6_DVFS_RV_VF1   (0x01)
#define SUN55IW6_DVFS_RV_VF1_1 (0x11)
#define SUN55IW6_DVFS_RV_VF2   (0x02)
#define SUN55IW6_DVFS_RV_VF2_1 (0x12)
#define SUN55IW6_DVFS_RV_VF2_3 (0x32)
#define SUN55IW6_DVFS_RV_VF2_4 (0x42)
#define SUN55IW6_DVFS_RV_VF2_5 (0x52)
#define SUN55IW6_DVFS_RV_VF2_6 (0x62)
#define SUN55IW6_DVFS_RV_VF3   (0x03)
#define SUN55IW6_DVFS_RV_VF4   (0x04)
#define SUN55IW6_DVFS_RV_VF4_1 (0x14)

/* sun55iw6 riscv vf table v0.95 */
struct vf_info vf_info_table[] = {
	/* dvfs                   voltage     core_rangle */
	{SUN55IW6_DVFS_RV_VF0,    920,        600000000},
#if !defined(CONFIG_AW_VF_FOR_T536)
	{SUN55IW6_DVFS_RV_VF1,    920,        480000000},
#endif
	{SUN55IW6_DVFS_RV_VF1,    940,        600000000},
	{SUN55IW6_DVFS_RV_VF1_1,  920,        480000000},
	{SUN55IW6_DVFS_RV_VF1_1,  940,        600000000},
	{SUN55IW6_DVFS_RV_VF2,    920,        480000000},
	{SUN55IW6_DVFS_RV_VF2,    940,        600000000},
	{SUN55IW6_DVFS_RV_VF2_1,  940,        600000000},
	{SUN55IW6_DVFS_RV_VF2_3,  940,        600000000},
	{SUN55IW6_DVFS_RV_VF2_4,  920,        480000000},
	{SUN55IW6_DVFS_RV_VF2_4,  940,        600000000},
	{SUN55IW6_DVFS_RV_VF2_5,  920,        480000000},
	{SUN55IW6_DVFS_RV_VF2_5,  940,        600000000},
	{SUN55IW6_DVFS_RV_VF2_6,  920,        480000000},
	{SUN55IW6_DVFS_RV_VF2_6,  940,        600000000},
	{SUN55IW6_DVFS_RV_VF3,    920,        600000000},
	{SUN55IW6_DVFS_RV_VF4,    920,        600000000},
	{SUN55IW6_DVFS_RV_VF4_1,  940,        600000000},
};

static int sunxi_riscv_check_vf_info(struct sunxi_rproc_riscv_cfg *cfg)
{
	struct udevice *dev;
	u32 voltage, i, ret;
	int dvfs_arry_size;
	u32 dvfs, bak_dvfs, combi_dvfs;

	ret = regulator_get_by_devname(cfg->regulator_name, &dev);
	if (ret) {
		pr_err("can not get riscv power supply\n");
		return -1;
	}

	voltage = regulator_get_value(dev) / 1000; //mv voltage
	pr_err("riscv read vdd-sys voltage:%d mv\n", voltage);

	dvfs = readl(SUNXI_SID_BASE + 0x200 + 0x20);
	bak_dvfs = (dvfs >> 24) & 0xff;
	if (bak_dvfs)
		combi_dvfs = bak_dvfs;
	else
		combi_dvfs = (dvfs >> 16) & 0xff;

	pr_err("riscv combi_dvfs:%d\n", combi_dvfs);

	dvfs_arry_size = sizeof(vf_info_table)/sizeof(struct vf_info);
	for (i = 0; i < dvfs_arry_size; i++) {
		if (vf_info_table[i].dvfs == combi_dvfs
				&& vf_info_table[i].voltage == voltage
				&& vf_info_table[i].core_rate_range >= cfg->core_rate)
		return 0;
	}

	pr_err("cannot find riscv vf point for %d freq and voltage %d!\n", cfg->core_rate, voltage);

	return -1;
}

int sunxi_rproc_start(struct udevice *dev)
{
	struct sunxi_rproc_privdata *priv = dev_get_priv(dev);
	struct sunxi_rproc_riscv_cfg *cfg;
	int ret;

	priv->driver_data = (void *)dev_get_driver_data(dev);
	cfg = &priv->driver_data->cfg;

	if (!priv->auto_boot) {
		pr_err("not need to start");
		return 0;
	}

	ret = reset_deassert(&cfg->sys_rst);
	if (ret < 0) {
		pr_err("%s sys reset assert failed: %d\n", __func__, ret);
		return -1;
	}

	ret = reset_deassert(&cfg->core_rst);
	if (ret < 0) {
		pr_err("%s core reset assert failed: %d\n", __func__, ret);
		goto err_deassert_sys_rst;
	}

	ret = reset_deassert(&cfg->cfg_rst);
	if (ret < 0) {
		pr_err("%s cfg reset assert failed: %d\n", __func__, ret);
		goto err_deassert_core_rst;
	}

	ret = clk_enable(&cfg->cfg_clk);
	if (ret < 0) {
		pr_err("clk_enable(cfg_clk) failed: %d\n", ret);
		goto err_deassert_cfg_rst;
	}

	writel(entry_addr, cfg->cfg_base + RISCV_STA_ADD_REG);

	ret = clk_enable(&cfg->ts_clk);
	if (ret < 0) {
		pr_err("clk_enable(ts_clk) failed: %d\n", ret);
		goto err_enable_cfg_clk;
	}

	ret = clk_set_parent(&cfg->core_clk, &cfg->input);
	if (ret < 0) {
		pr_err("clk_set_parent(core_clk) failed: %d\n", ret);
		goto err_enable_ts_clk;
	}

	ret = clk_set_rate(&cfg->core_clk, cfg->core_rate);
	if (ret < 0) {
		pr_err("clk_set_rate(core_clk) failed: %d\n", ret);
		goto err_enable_ts_clk;
	}

	ret = clk_set_rate(&cfg->axi_clk, cfg->axi_rate);
	if (ret < 0) {
		pr_err("clk_set_rate(axi_clk) failed: %d\n", ret);
		goto err_enable_ts_clk;
	}

	ret = clk_enable(&cfg->core_clk);
	if (ret < 0) {
		pr_err("clk_enable(core_clk) failed: %d\n", ret);
		goto err_enable_ts_clk;
	}

	pr_err("%s succuss!\n", __func__);
	priv->running = true;

	return 0;

err_enable_ts_clk:
	clk_disable(&cfg->ts_clk);
err_enable_cfg_clk:
	clk_disable(&cfg->cfg_clk);
err_deassert_cfg_rst:
	reset_assert(&cfg->cfg_rst);
err_deassert_sys_rst:
	reset_assert(&cfg->sys_rst);
err_deassert_core_rst:
	reset_free(&cfg->core_rst);

	return -1;
}

int rproc_priv_parser_resource(struct udevice *dev)
{
	struct sunxi_rproc_privdata *priv = dev_get_priv(dev);
	struct sunxi_rproc_riscv_cfg *cfg;
	int ret;

	priv->dev = dev;
	priv->uc_pdata = dev_get_uclass_plat(dev);
	priv->driver_data = (void *)dev_get_driver_data(dev);

	cfg = &priv->driver_data->cfg;

	cfg->cfg_base = (void *)dev_read_addr_index_ptr(dev, 0);
	if (!cfg->cfg_base) {
		pr_err("get cfg_base fail\n");
		return -1;
	}

	ret = reset_get_by_name(dev, "rv-sys-rst", &cfg->sys_rst);
	if (ret) {
		pr_err("rproc get sys-rst fail\n");
		return -1;
	}

	ret = reset_get_by_name(dev, "core-rst", &cfg->core_rst);
	if (ret) {
		pr_err("rproc get core-rst fail\n");
		goto err_free_sys_rst;
	}

	ret = reset_get_by_name(dev, "rv-cfg-rst", &cfg->cfg_rst);
	if (ret) {
		pr_err("rproc get rv-cfg-rst fail\n");
		goto err_free_core_rst;
	}

	//parser from dts
	ret = clk_get_by_name(dev, "input", &cfg->input);
	if (ret) {
		pr_err("rproc clk_get_by_name(input) failed: %d\n", ret);
		goto err_free_cfg_rst;
	}

	ret = clk_get_by_name(dev, "core-gate", &cfg->core_clk);
	if (ret) {
		pr_err("rproc clk_get_by_name(core-gate) failed: %d\n", ret);
		goto err_free_input_clk;
	}

	ret = clk_get_by_name(dev, "rv-cfg-gate", &cfg->cfg_clk);
	if (ret) {
		pr_err("rproc clk_get_by_name(rv-cfg-gate) failed: %d\n", ret);
		goto err_free_core_clk;
	}

	ret = clk_get_by_name(dev, "rv-ts-gate", &cfg->ts_clk);
	if (ret) {
		pr_err("rproc clk_get_by_name(rv-ts-gate) failed: %d\n", ret);
		goto err_free_cfg_clk;
	}

	ret = clk_get_by_name(dev, "axi-div", &cfg->axi_clk);
	if (ret) {
		pr_err("rproc clk_get_by_name(axi-div) failed: %d\n", ret);
		goto err_free_ts_clk;
	}

	cfg->core_rate = dev_read_u32_default(dev, "core-clock-frequency", DEFAULT_CORE_RATE);

	cfg->axi_rate = dev_read_u32_default(dev, "axi-clock-frequency", DEFAULT_AXI_RATE);

	cfg->regulator_name = dev_read_string(dev, "regulator-name");
	ret = sunxi_riscv_check_vf_info(cfg);

	pr_info("%s get core_rate:%x axi_rate:%x\n", __func__, cfg->core_rate, cfg->axi_rate);

	return ret;

err_free_ts_clk:
	clk_free(&cfg->ts_clk);
err_free_cfg_clk:
	clk_free(&cfg->cfg_clk);
err_free_core_clk:
	clk_free(&cfg->core_clk);
err_free_input_clk:
	clk_free(&cfg->input);
err_free_cfg_rst:
	reset_free(&cfg->cfg_rst);
err_free_core_rst:
	reset_free(&cfg->core_rst);
err_free_sys_rst:
	reset_free(&cfg->sys_rst);

	return -1;
}

int get_aw_rproc_boot_state(int *is_booted)
{
	/*
	0x02002000
	0xB80
	bit 31
	*/
	uint32_t reg_addr, reg_data;

	if (!is_booted)
		return -1;

	reg_addr = 0x02002000 + 0xB80;
	reg_data = readl(reg_addr);
	if (reg_data & (1 << 31))
		*is_booted = 1;
	else
		*is_booted = 0;

	return 0;
}
