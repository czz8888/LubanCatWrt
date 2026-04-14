// SPDX-License-Identifier: GPL-2.0+
/*
 * drivers/remoteproc/rproc-e907-sun8iw22.c
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

extern ulong entry_addr;

#if 0
static struct vf_info vf_info_table[] = {
	/* core rate freq		voltage(mv) */
	{600000000,				900},
};

static int sunxi_riscv_check_vf_info(struct sunxi_rproc_riscv_cfg *cfg)
{
	struct udevice *dev;
	u32 voltage, i, ret;
	int array_size = sizeof(vf_info_table)/sizeof(struct vf_info);

	ret = regulator_get_by_devname("dcdc1", &dev);
	if (ret) {
		pr_err("can not get riscv power supply\n");
		return -1;
	}

	voltage = regulator_get_value(dev) / 1000; //mv voltage
	pr_info("read vdd-sys voltage:%d mv\n", voltage);

	for (i = 0; i < array_size; i++) {
		if (cfg->core_rate == vf_info_table[i].core_freq) {
			if (voltage != vf_info_table[i].voltage) {
				pr_err("check vf info error, core_freq:%d, vol should be:%d\n",
					vf_info_table[i].core_freq,
					vf_info_table[i].voltage);
				cfg->vf_check_result = false;
				return -1;
			} else {
				cfg->vf_check_result = true;
				break;
			}
		}
	}

	if (i == array_size) {
		pr_err("not found core rate in vf table!\n");
		return -1;
	}

	return 0;
}
#endif

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

	ret = reset_deassert(&cfg->cfg_rst);
	if (ret < 0) {
		pr_err("%s cfg reset assert failed: %d\n", __func__, ret);
		return -1;
	}

	ret = clk_enable(&cfg->cfg_clk);
	if (ret < 0) {
		pr_err("clk_enable(cfg_clk) failed: %d\n", ret);
		goto err_deassert_cfg_rst;
	}

	ret = clk_set_parent(&cfg->core_clk, &cfg->input);
	if (ret < 0) {
		pr_err("clk_set_parent(core_clk) failed: %d\n", ret);
		goto err_enable_cfg_clk;
	}

	ret = clk_set_rate(&cfg->core_clk, cfg->core_rate);
	if (ret < 0) {
		pr_err("clk_set_rate(core_clk) failed: %d\n", ret);
		goto err_enable_cfg_clk;
	}

	ret = clk_set_rate(&cfg->axi_clk, cfg->axi_rate);
	if (ret < 0) {
		pr_err("clk_set_rate(axi_clk) failed: %d\n", ret);
		goto err_enable_cfg_clk;
	}

	ret = clk_enable(&cfg->axi_mon_clk);
	if (ret < 0) {
		pr_err("clk_enable(cfg_axi_mon) failed: %d\n", ret);
		goto err_enable_cfg_clk;
	}

	ret = reset_deassert(&cfg->axi_mon_rst);
	if (ret < 0) {
		pr_err("%s axi_mon_rst assert failed: %d\n", __func__, ret);
		goto err_enable_axi_mon_clk;
	}
	writel(AXI_MONITOR_TAKEN_OVER_ENABLE,
		cfg->axi_monitor_base + AXI_MONITOR_TAKEN_OVER_REG);
	writel(AXI_MONITOR_TAKEN_OVER_TIMEOUT_TIME,
		cfg->axi_monitor_base + AXI_MONITOR_TAKEN_TIMEOUT_REG);
	writel(entry_addr, cfg->cfg_base + RISCV_STA_ADD_REG);

	ret = clk_enable(&cfg->core_clk);
	if (ret < 0) {
		pr_err("clk_enable(core_clk) failed: %d\n", ret);
		goto err_deassert_axi_mon_rst;
	}

	ret = clk_enable(&cfg->ts_clk);
	if (ret < 0) {
		pr_err("clk_enable(ts_clk) failed: %d\n", ret);
		goto err_enable_core_clk;
	}

	ret = reset_deassert(&cfg->sys_rst);
	if (ret < 0) {
		pr_err("%s sys reset assert failed: %d\n", __func__, ret);
		goto err_enable_ts_clk;
	}

	ret = reset_deassert(&cfg->core_rst);
	if (ret < 0) {
		pr_err("%s core reset assert failed: %d\n", __func__, ret);
		goto err_deassert_sys_rst;
	}

	priv->running = true;
	return 0;

err_deassert_sys_rst:
	reset_assert(&cfg->sys_rst);
err_enable_ts_clk:
	clk_disable(&cfg->ts_clk);
err_enable_core_clk:
	clk_disable(&cfg->core_clk);
err_deassert_axi_mon_rst:
	reset_assert(&cfg->axi_mon_rst);
err_enable_axi_mon_clk:
	clk_disable(&cfg->axi_mon_clk);
err_enable_cfg_clk:
	clk_disable(&cfg->cfg_clk);
err_deassert_cfg_rst:
	reset_assert(&cfg->cfg_rst);

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

	cfg->axi_monitor_base = (void *)dev_read_addr_index_ptr(dev, 1);
	if (!cfg->axi_monitor_base) {
		pr_err("rproc get axi_monitor_base fail\n");
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

	ret = clk_get_by_name(dev, "axi-monitor", &cfg->axi_mon_clk);
	if (ret) {
		pr_err("rproc clk_get_by_name(axi-div) failed: %d\n", ret);
		goto err_free_axi_clk;
	}

	ret = reset_get_by_name(dev, "rst-axi-monitor", &cfg->axi_mon_rst);
	if (ret) {
		pr_err("rproc clk_get_by_name(axi-div) failed: %d\n", ret);
		goto err_free_axi_mon_clk;
	}

	cfg->core_rate = dev_read_u32_default(dev, "core-clock-frequency", DEFAULT_CORE_RATE);

	cfg->axi_rate = dev_read_u32_default(dev, "axi-clock-frequency", DEFAULT_AXI_RATE);

#if 0
	cfg->regulator_name = dev_read_string(dev, "regulator-name");
	sunxi_riscv_check_vf_info(cfg);
#endif

	pr_info("%s get core_rate:%d axi_rate:%d\n", __func__, cfg->core_rate, cfg->axi_rate);

	return 0;

err_free_axi_mon_clk:
	clk_free(&cfg->axi_mon_clk);
err_free_axi_clk:
	clk_free(&cfg->axi_clk);
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
