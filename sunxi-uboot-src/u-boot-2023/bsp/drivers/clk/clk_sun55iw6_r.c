/*
 * (C) Copyright 2013-2016
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <clk-uclass.h>
#include <dm.h>
#include <errno.h>
#include <dt-bindings/clock/sun55iw6p1-r-ccu.h>
#include <dt-bindings/reset/sun55iw6p1-r-ccu.h>
#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <clk/sunxi.h>
#include "sunxi-clk.h"

static struct ccu_reset sunxi_resets[] = {
	[RST_BUS_SPI]		= RESET(0x015c, BIT(16)),
	[RST_BUS_R_TWI0]		= RESET(0x019c, BIT(16)),
	[RST_BUS_R_TWI1]		= RESET(0x019c, BIT(17)),
};

static const char * const spi_parents[] = {
	"osc24M",
	"dcxo", "perippl-div",
	"pll-peri0-300m", "pll-peri1-300m"
};

static const char * const ahbs_parents[] = {
	"osc24M",
	"dcxo", "rtc-32k", "rc16m",
	"peripll-div-200m", "peri0-300m"
};

static const char * const apbs0_parents[] = {
	"osc24M",
	"dcxo", "rtc-32k", "rc16m",
	"peripll-dev200m"
};

static const char * const apbs1_parents[] = {
	"osc24M",
	"dcxo", "rtc-32k", "rc16m",
	"peripll-dev200m"
};

enum enum_mux_cfg {
	MUX_CLK_AHBS,
	MUX_CLK_APBS0,
	MUX_CLK_APBS1,

	MUX_CLK_SPI,
};

static const struct sunxi_mux_cfg sun55iw6_muxes[] = {
	MUX_CFG(MUX_CLK_AHBS, ahbs_parents, 0x0000, 24, 3),
	MUX_CFG(MUX_CLK_APBS0, apbs0_parents, 0x000c, 24, 3),
	MUX_CFG(MUX_CLK_APBS1, apbs1_parents, 0x0010, 24, 3),

	MUX_CFG(MUX_CLK_SPI, spi_parents, 0x0150, 24, 3),
};

enum enum_gate_cfg {
	GATE_CLK_BUS_SPI,
	GATE_CLK_SPI,
	GATE_CLK_R_TWI0,
	GATE_CLK_R_TWI1,
};

static const struct sunxi_gate_cfg sun55iw6_gates[] = {
	GATE_CFG(GATE_CLK_BUS_SPI, 0x015c, BIT(0)),
	GATE_CFG(GATE_CLK_SPI, 0x0150, BIT(31)),
	GATE_CFG(GATE_CLK_R_TWI0, 0x019c, BIT(0)),
	GATE_CFG(GATE_CLK_R_TWI1, 0x019c, BIT(1)),
};

enum enum_div_cfg {
	DIV_CLK_AHBS,
	DIV_CLK_APBS0,
	DIV_CLK_APBS1,

	DIV_CLK_SPI,
};

static const struct sunxi_div_cfg sun55iw6_dividers[] = {
	DIV_CFG(DIV_CLK_AHBS, 0x0000, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_APBS0, 0x000c, 0, 5, 0, 0, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_APBS1, 0x0010, 0, 5, 0, 0, 0, 0, 0, NULL),

	DIV_CFG(DIV_CLK_SPI, 0x0150, 0, 5, 8, 5, 0, 0, 0, NULL),
};

static const struct clock_config sun55iw6_clock_cfg[] = {
	SUNXI_CLK_COMPOSITE(CLK_AHBS, "ahbs", 0, NO_SUNXI_GATE, MUX_CLK_AHBS, DIV_CLK_AHBS),
	SUNXI_CLK_COMPOSITE(CLK_APBS0, "apbs0", 0, NO_SUNXI_GATE, MUX_CLK_APBS0, DIV_CLK_APBS0),
	SUNXI_CLK_COMPOSITE(CLK_APBS1, "apbs1", 0, NO_SUNXI_GATE, MUX_CLK_APBS1, DIV_CLK_APBS1),

	SUNXI_CLK_GATE(CLK_BUS_SPI, "spi-bus", "osc24M", 0, GATE_CLK_BUS_SPI),
	SUNXI_CLK_COMPOSITE(CLK_SPI, "spi", 0, GATE_CLK_SPI, MUX_CLK_SPI, DIV_CLK_SPI),

	SUNXI_CLK_GATE(CLK_BUS_R_TWI0, "s_twi0", "osc24M", 0, GATE_CLK_R_TWI0),
	SUNXI_CLK_GATE(CLK_BUS_R_TWI1, "s_twi1", "osc24M", 0, GATE_CLK_R_TWI1),
};

const struct ccu_desc sunxi_r_ccu_desc = {
	.resets = sunxi_resets,
	.num_resets = ARRAY_SIZE(sunxi_resets),
};

static const struct sunxi_clock_match_data sun55iw6_data = {
	.tab_clocks	= sun55iw6_clock_cfg,
	.num_clocks	= ARRAY_SIZE(sun55iw6_clock_cfg),
	.clock_data = &(const struct clk_sunxi_clock_data) {
		.num_gates	= ARRAY_SIZE(sun55iw6_gates),
		.gates		= sun55iw6_gates,
		.muxes		= sun55iw6_muxes,
		.dividers	= sun55iw6_dividers,
	},
};

static int sun55iw6_clk_probe(struct udevice *dev)
{
	int err;

	err = sunxi_clk_init(dev, &sun55iw6_data);
	if (err)
		return err;

	return 0;
}

static const struct udevice_id sun55iw6_clk_r_ids[] = {
	{ .compatible = "allwinner,sunxi-r-ccu",
	  .data = (ulong)&sunxi_r_ccu_desc },
	{ }
};

U_BOOT_DRIVER(sun55iw6_clk_r) = {
	.name		= "sun55iw6_clk_r",
	.id		= UCLASS_CLK,
	.of_match	= sun55iw6_clk_r_ids,
	.bind		= sunxi_clk_bind,
	.probe		= sun55iw6_clk_probe,
	.of_to_plat	= sunxi_clk_of_to_plat,
	.ops 		= &sunxi_ccu_ops,
	.priv_auto 	= sizeof(struct sunximp_rcc_priv),
};
