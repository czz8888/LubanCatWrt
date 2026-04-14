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
#include <clk/sunxi.h>
#include <dt-bindings/clock/sun8iw22p1-ccu.h>
#include <dt-bindings/reset/sun8iw22p1-ccu.h>
#include <linux/bitops.h>
#include <asm/arch/clock.h>
#include <linux/clk-provider.h>
#include "sunxi-clk.h"
static struct ccu_reset sunxi_resets[] = {
	[RST_BUS_MMC0]		= RESET(0xd0c, BIT(16)),
	[RST_BUS_MMC1]		= RESET(0xd1c, BIT(16)),
	[RST_BUS_MMC2]		= RESET(0xd2c, BIT(16)),
	[RST_BUS_UART0]		= RESET(0xe00, BIT(16)),
};

static const char * const mmc0_parents[] = {
	"osc24M", "pll-peri0-400m", "pll-peri0-300m", "pll-peri1-400m", "pll-peri1-300m"
};

static const char * const mmc1_parents[] = {
	"osc24M", "pll-peri0-400m", "pll-peri0-300m", "pll-peri1-400m", "pll-peri1-300m"
};

static const char * const mmc2_parents[] = {
	"osc24M", "pll-peri0-800m", "pll-peri0-600m", "pll-peri1-800m", "pll-peri1-600m"
};

static const char * const apb1_parents[] = {
	"osc24M", "ext32k", "rc16m", "pll-peri0-600m"
};

enum enum_mux_cfg {
	MUX_CLK_MMC0,
	MUX_CLK_MMC1,
	MUX_CLK_MMC2,
	MUX_APB1,
};

static const struct sunxi_mux_cfg sun8iw22_muxes[] = {
	MUX_CFG(MUX_CLK_MMC0, mmc0_parents, 0xd00, 24, 3),
	MUX_CFG(MUX_CLK_MMC1, mmc1_parents, 0xd10, 24, 3),
	MUX_CFG(MUX_CLK_MMC2, mmc2_parents, 0xd20, 24, 3),
	MUX_CFG(MUX_APB1, apb1_parents, 0x518, 24, 2),
};

enum enum_gate_cfg {
	GATE_CLK_PLL_PERIPH0,
	GATE_CLK_PLL_PERIPH1,
	GATE_CLK_BUS_MMC0,
	GATE_CLK_BUS_MMC1,
	GATE_CLK_BUS_MMC2,
	GATE_CLK_MMC0,
	GATE_CLK_MMC1,
	GATE_CLK_MMC2,
	GATE_CLK_UART0,
};


static const struct sunxi_gate_cfg sun8iw22_gates[] = {
	GATE_CFG(GATE_CLK_PLL_PERIPH0, 0xA0, BIT(31)),
	GATE_CFG(GATE_CLK_PLL_PERIPH1, 0xC0, BIT(31)),
	GATE_CFG(GATE_CLK_MMC0, 0x0d00, BIT(31)),
	GATE_CFG(GATE_CLK_MMC1, 0x0d10, BIT(31)),
	GATE_CFG(GATE_CLK_MMC2, 0x0d20, BIT(31)),
	GATE_CFG(GATE_CLK_BUS_MMC0, 0x0d0c, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_MMC1, 0x0d1c, BIT(0)),
	GATE_CFG(GATE_CLK_BUS_MMC2, 0x0d2c, BIT(0)),
	GATE_CFG(GATE_CLK_UART0, 0x0e00, BIT(0)),
};

enum enum_div_cfg {
	DIV_CLK_PLL_PERIPH0_2X,
	DIV_CLK_PLL_PERIPH0_800M,
	DIV_CLK_PLL_PERIPH0_480M,
	DIV_CLK_PLL_PERIPH1_2X,
	DIV_CLK_PLL_PERIPH1_800M,
	DIV_CLK_PLL_PERIPH1_480M,
	DIV_CLK_MMC0,
	DIV_CLK_MMC1,
	DIV_CLK_MMC2,
	DIV_CLK_APB1,
};

static const struct sunxi_div_cfg sun8iw22_dividers[] = {
	DIV_CFG(DIV_CLK_PLL_PERIPH0_2X,   0xa0, 1, 1, 16, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0_800M, 0xa0, 1, 1, 20, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH0_480M, 0xa0, 1, 1, 2, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH1_2X,   0xc0, 1, 1, 16, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH1_800M, 0xc0, 1, 1, 20, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_PLL_PERIPH1_480M, 0xc0, 1, 1, 2, 3, 8, 8, 0, NULL),
	DIV_CFG(DIV_CLK_MMC0, 0xD00, 0, 5, 8, 5, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_MMC1, 0xD10, 0, 5, 8, 5, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_MMC2, 0xD20, 0, 5, 8, 5, 0, 0, 0, NULL),
	DIV_CFG(DIV_CLK_APB1, 0x518, 0, 5, 0, 0, 0, 0, 0, NULL),
};


static const struct clock_config sun8iw22_clock_cfg[] = {
	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0, "pll-peri0-2x", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0_2X),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0_800M, "pll-peri0-800m", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0_800M),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH0_480M, "pll-peri0-480m", "osc24M", 0,
			GATE_CLK_PLL_PERIPH0, DIV_CLK_PLL_PERIPH0_480M),

	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_600M, "pll-peri0-600m", "pll-peri0-2x", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_300M, "pll-peri0-300m", "pll-peri0-600m", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH0_400M, "pll-peri0-400m", "pll-peri0-2x", 3, 1, 0),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH1, "pll-peri1-2x", "osc24M", 0,
			GATE_CLK_PLL_PERIPH1, DIV_CLK_PLL_PERIPH1_2X),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH1_800M, "pll-peri1-800m", "osc24M", 0,
			GATE_CLK_PLL_PERIPH1, DIV_CLK_PLL_PERIPH1_800M),

	SUNXI_CLK_COMPOSITE_FACTOR(CLK_PLL_PERIPH1_480M, "pll-peri1-480m", "osc24M", 0,
			GATE_CLK_PLL_PERIPH1, DIV_CLK_PLL_PERIPH1_480M),

	SUNXI_CLK_FIXED(CLK_PLL_PERIPH1_600M, "pll-peri1-600m", "pll-peri1-2x", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH1_300M, "pll-peri1-300m", "pll-peri1-600m", 2, 1, 0),
	SUNXI_CLK_FIXED(CLK_PLL_PERIPH1_400M, "pll-peri1-400m", "pll-peri1-2x", 3, 1, 0),

	SUNXI_CLK_COMPOSITE(CLK_MMC0, "mmc0", 0,
			GATE_CLK_MMC0, MUX_CLK_MMC0, DIV_CLK_MMC0),
	SUNXI_CLK_COMPOSITE(CLK_MMC1, "mmc1", 0,
			GATE_CLK_MMC1, MUX_CLK_MMC1, DIV_CLK_MMC1),
	SUNXI_CLK_COMPOSITE(CLK_MMC2, "mmc2", 0,
			GATE_CLK_MMC2, MUX_CLK_MMC2, DIV_CLK_MMC2),
	SUNXI_CLK_GATE(CLK_BUS_MMC0, "mmc0-bus", "osc24M", 0, GATE_CLK_BUS_MMC0),
	SUNXI_CLK_GATE(CLK_BUS_MMC1, "mmc1-bus", "osc24M", 0, GATE_CLK_BUS_MMC1),
	SUNXI_CLK_GATE(CLK_BUS_MMC2, "mmc2-bus", "osc24M", 0, GATE_CLK_BUS_MMC2),
	SUNXI_CLK_GATE(CLK_BUS_UART0, "uart0-bus", "osc24M", 0, GATE_CLK_UART0),

	SUNXI_CLK_COMPOSITE(CLK_APB1, "apb1", CLK_SET_RATE_PARENT,
			NO_SUNXI_GATE, MUX_APB1, DIV_CLK_APB1),

};


const struct ccu_desc sunxi_ccu_desc = {
	.resets = sunxi_resets,
	.num_resets = ARRAY_SIZE(sunxi_resets),
};


static const struct sunxi_clock_match_data sun8iw22_data = {
	.tab_clocks	= sun8iw22_clock_cfg,
	.num_clocks	= ARRAY_SIZE(sun8iw22_clock_cfg),
	.clock_data = &(const struct clk_sunxi_clock_data) {
		.num_gates	= ARRAY_SIZE(sun8iw22_gates),
		.gates		= sun8iw22_gates,
		.muxes		= sun8iw22_muxes,
		.dividers	= sun8iw22_dividers,
	},
};

static int sun8iw22_clk_probe(struct udevice *dev)
{
	int err;

	err = sunxi_clk_init(dev, &sun8iw22_data);
	if (err)
		return err;

	return 0;
}

static const struct udevice_id sun8iw22_clk_ids[] = {
	{ .compatible = "allwinner,sunxi-ccu",
	  .data = (ulong)&sunxi_ccu_desc },
	{ }
};

U_BOOT_DRIVER(sun8iw22_clk) = {
	.name		= "sun8iw22_clk",
	.id		= UCLASS_CLK,
	.of_match	= sun8iw22_clk_ids,
	.bind		= sunxi_clk_bind,
	.probe		= sun8iw22_clk_probe,
	.of_to_plat	= sunxi_clk_of_to_plat,
	.ops 		= &sunxi_ccu_ops,
	.priv_auto 	= sizeof(struct sunximp_rcc_priv),
};

