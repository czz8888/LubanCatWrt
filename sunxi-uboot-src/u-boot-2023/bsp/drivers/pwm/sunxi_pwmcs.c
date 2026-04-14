// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2025 AllWinner
 */

#include <common.h>
#include <div64.h>
#include <dm.h>
#include <log.h>
#include <pwm.h>
#include <regmap.h>
#include <syscon.h>
#include <asm/global_data.h>
#include <asm/io.h>
#include <asm/arch/pwm.h>
#include <power/regulator.h>
#include <clk.h>
#include <reset.h>
#include <div64.h>
#include "sunxi_pwmcs.h"

DECLARE_GLOBAL_DATA_PTR;

#define sunxi_pwm_debug 0
#undef  sunxi_pwm_debug

#ifdef sunxi_pwm_debug
	#define pwm_debug(fmt, args...)	printf(fmt, ##args)
#else
	#define pwm_debug(fmt, args...)
#endif

#define PWM_PIN_STATE_ACTIVE "active"
#define PWM_PIN_STATE_SLEEP "sleep"

#define SETMASK(width, shift)   ((width ? ((-1U) >> (32 - width)) : 0)  << (shift))
#define CLRMASK(width, shift)   (~(SETMASK(width, shift)))
#define GET_BITS(shift, width, reg)     \
	    (((reg) & SETMASK(width, shift)) >> (shift))
#define SET_BITS(shift, width, reg, val) \
	    (((reg) & CLRMASK(width, shift)) | (val << (shift)))

#define PRESCALE_MAX 256
#define SUNXI_CLK_400M		400000000
#define SUNXI_CLK_100M		100000000
#define SUNXI_CLK_24M		24000000
#define SUNXI_DIV_CLK		1000000000

static u32 sunxi_pwm_pre_scal[][2] = {
	/* reg_value  clk_pre_div */
	{0, 1},
	{1, 2},
	{2, 4},
	{3, 8},
	{4, 16},
	{5, 32},
	{6, 64},
	{7, 128},
	{8, 256},
};

struct sunxi_pwm_hw_data {
	u32 pwm_reg_uniform_offset;
};

struct sunxi_pwmcs_priv {
	u32 pwm_base;
	bool invert;
	u32 prescaler;
	struct reset_ctl reset;
	struct clk clk;
	const struct sunxi_pwm_hw_data *data;
};

static inline u32 sunxi_pwm_readl(u32 base, u32 offset)
{
	unsigned int value;

	value = readl((void *)(unsigned long)(base + offset));

	return value;
}

static inline u32 sunxi_pwm_writel(u32 base, u32 offset, u32 value)
{
	writel(value, (void *)(unsigned long)(base + offset));

	return 0;
}

static int sunxi_pwmcs_set_invert(struct udevice *dev, uint channel,
				bool polarity)
{
	unsigned int temp;
	unsigned int reg_offset, reg_shift, reg_width;
	struct sunxi_pwmcs_priv *priv = dev_get_plat(dev);
	u32 reg_base = priv->pwm_base;

	reg_shift = PWMCS_ACT_STA_SHIFT;
	reg_width = PWMCS_ACT_STA_WIDTH;

	reg_offset = PWMCS_CH_REG_ADDR(channel, PCS_PCR) + PWMCS_CH_UNIFORM_OFFSET(channel);
	temp = sunxi_pwm_readl(reg_base, reg_offset);

	if (polarity)
		temp = SET_BITS(reg_shift, reg_width, temp, 0);
	else
		temp = SET_BITS(reg_shift, reg_width, temp, 1);

	sunxi_pwm_writel(reg_base, reg_offset, temp);

	return 0;
}

static int sunxi_pwmcs_set_config(struct udevice *dev, uint channel,
				uint period_ns, uint duty_ns)
{
	unsigned int temp;
	unsigned int sel = channel;
	unsigned int value;
	unsigned int reg_offset, reg_shift, reg_width;
	unsigned int reg_clk_src_shift, reg_clk_src_width;
	unsigned int reg_div_m_shift, reg_div_m_width;
	unsigned int pre_scal_id = 0, div_m = 0, prescale = 0;
	unsigned long long clk_src = 0;
	unsigned long entire_cycles = 256, active_cycles = 192;

	struct sunxi_pwmcs_priv *priv = dev_get_plat(dev);
	u32 reg_base = priv->pwm_base;

	temp = readl(reg_base + PWMCS_O_MULT_CFG);
	temp |= (0x1 << sel);
	writel(temp, reg_base + PWMCS_O_MULT_CFG);

	reg_clk_src_shift = PWMCS_CLK_SRC_SHIFT;
	reg_clk_src_width = PWMCS_CLK_SRC_WIDTH;

	if (period_ns > 0 && period_ns <= 10) {
		clk_src = SUNXI_CLK_400M;

		reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCCR01);
		/* clk_src_reg */
		temp = readl(reg_base + reg_offset);
		temp = SET_BITS(reg_clk_src_shift, reg_clk_src_width, temp, 2); /* select clock source 400M */
		writel(temp, reg_base + reg_offset);

		reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCGR);
		temp = readl(reg_base + PCS_PCGR);
		temp = SET_BITS(0, 1, temp, 1); /* clk_gating set */
		temp = SET_BITS(16, 1, temp, 1); /* clk_bypass set */
		writel(temp, reg_base + reg_offset);
	} else if (period_ns > 10 && period_ns <= 334) {
		clk_src = SUNXI_CLK_100M;

		reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCCR01);
		temp = readl(reg_base + reg_offset);
		temp = SET_BITS(reg_clk_src_shift, reg_clk_src_width, temp, 1);
		writel(temp, reg_base + reg_offset);
	} else if (period_ns > 334) {
		/* if freq < 3M, then select 24M clock */
		clk_src = SUNXI_CLK_24M;

		reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCCR01);
		/* clk_src_reg : use OSC24M clock */
		temp = readl(reg_base + reg_offset);
		temp = SET_BITS(reg_clk_src_shift, reg_clk_src_width, temp, 0);
		writel(temp, reg_base + reg_offset);

		reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCGR);
		temp = readl(reg_base + PCS_PCGR);
		reg_shift = sel%2;
		temp = SET_BITS(reg_shift, 1, temp, 1); /* clk_gating set */
		writel(temp, reg_base + reg_offset);
	}

	clk_src = clk_src * period_ns;
	do_div(clk_src, SUNXI_DIV_CLK);
	entire_cycles = (unsigned long)clk_src;  /* How many clksrc beats in a PWM period */

	/* get entire cycle length */
	for (pre_scal_id = 0; pre_scal_id < 9; pre_scal_id++) {
		if (entire_cycles <= 65536)
			break;
		for (prescale = 0; prescale < PRESCALE_MAX+1; prescale++) {
			entire_cycles = ((unsigned long)clk_src/sunxi_pwm_pre_scal[pre_scal_id][1])/(prescale + 1);
			if (entire_cycles <= 65536) {
				div_m = sunxi_pwm_pre_scal[pre_scal_id][0];
				break;
			}
		}
	}

	clk_src = (unsigned long long)entire_cycles * duty_ns;
	do_div(clk_src, period_ns);
	active_cycles = clk_src;
	if (entire_cycles == 0)
		entire_cycles++;

	/* config clk div_m */
	reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCCR01);
	reg_div_m_shift = PWMCS_DIV_M_SHIFT;
	reg_div_m_width = PWMCS_DIV_M_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_div_m_shift, reg_div_m_width, temp, div_m);
	writel(temp, reg_base + reg_offset);

	/* config gating */
	reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCGR);
	reg_shift = sel%2;
	value = readl(reg_base + reg_offset);
	value = SET_BITS(reg_shift, 1, value, 1); /* set gating */
	writel(value, reg_base + reg_offset);

	/* config prescal */
	reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCR) + PWMCS_CH_UNIFORM_OFFSET(sel);
	reg_shift = PWMCS_PRESCAL_SHIFT;
	reg_width = PWMCS_PRESCAL_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, prescale);
	writel(temp, reg_base + reg_offset);

	reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PCR) + PWMCS_CH_UNIFORM_OFFSET(sel);
	reg_shift = PWMCS_CFG_MODE_SHIFT;
	reg_width = PWMCS_CFG_MODE_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, 1);
	writel(temp, reg_base + reg_offset);

	/* config active cycles */
	reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PWM_ACT_CYCLE) + PWMCS_CH_UNIFORM_OFFSET(sel);
	reg_shift = PWMCS_ACT_CYCLES_SHIFT;
	reg_width = PWMCS_ACT_CYCLES_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, active_cycles);
	writel(temp, reg_base + reg_offset);

	/* config entire cycles */
	reg_offset = PWMCS_CH_REG_ADDR(sel, PCS_PWM_ENTIRE_CY) + PWMCS_CH_UNIFORM_OFFSET(sel);
	reg_shift = PWMCS_ENTIRE_CYCLES_SHIFT;
	reg_width = PWMCS_ENTIRE_CYCLES_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, (entire_cycles - 1));
	writel(temp, reg_base + reg_offset);

	pwm_debug("PWM %s: duty_ns=%d, period_ns=%d\n", __func__, duty_ns, period_ns);

	return 0;
}

static int sunxi_pwmcs_disable_channel(u32 reg_base, uint channel)
{
	unsigned int temp;
	unsigned int reg_offset, reg_shift, reg_width;

	/* disable pwm channel*/
	reg_offset = PWMCS_CH_REG_ADDR(channel, PCS_PER);
	reg_shift = PWMCS_OUT_EN_SHIFT;
	reg_width = PWMCS_OUT_EN_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, 0);
	writel(temp, reg_base + reg_offset);

	return 0;
}

static int sunxi_pwmcs_enable_channel(u32 reg_base, uint channel)
{
	unsigned int temp;
	unsigned int reg_offset, reg_shift, reg_width;

	/* pwm channel enable */
	reg_offset = PWMCS_CH_REG_ADDR(channel, PCS_PER);
	reg_shift = PWMCS_OUT_EN_SHIFT;
	reg_width = PWMCS_OUT_EN_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, 1);
	writel(temp, reg_base + reg_offset);

	reg_offset = PWMCS_CH_REG_ADDR(channel, PCS_PCR) + PWMCS_CH_UNIFORM_OFFSET(channel);
	reg_shift = PWMCS_CFG_MODE_SHIFT;
	reg_width = PWMCS_CFG_MODE_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, 1);
	writel(temp, reg_base + reg_offset);

	reg_shift = PWMCS_NEW_DATA_SHIFT;
	reg_width = PWMCS_NEW_DATA_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, 1);
	writel(temp, reg_base + reg_offset);

	reg_shift = PWMCS_UPDATE_MODE_SHIFT;
	reg_width = PWMCS_UPDATE_MODE_WIDTH;
	temp = readl(reg_base + reg_offset);
	temp = SET_BITS(reg_shift, reg_width, temp, 1);
	writel(temp, reg_base + reg_offset);

	return 0;
}

static int sunxi_pwmcs_set_enable(struct udevice *dev, uint channel, bool enable)
{
	struct sunxi_pwmcs_priv *priv = dev_get_plat(dev);
	u32 reg_base = priv->pwm_base;

	if (enable) {
		sunxi_pwmcs_enable_channel(reg_base, channel);
	} else {
		sunxi_pwmcs_disable_channel(reg_base, channel);
	}

	return 0;
}

static int sunxi_pwmcs_of_to_plat(struct udevice *dev)
{
	struct sunxi_pwmcs_priv *priv = dev_get_plat(dev);

	priv->pwm_base = dev_read_addr(dev);

	return 0;
}

static int sunxi_pwmcs_probe(struct udevice *dev)
{
	struct sunxi_pwmcs_priv *priv = dev_get_plat(dev);

	int ret;

	ret = reset_get_by_name(dev, "rst", &priv->reset);
	if (ret)
		return ret;

	reset_assert(&priv->reset);
	udelay(2);
	reset_deassert(&priv->reset);

	ret = clk_get_by_name(dev, "gate", &priv->clk);
	if (ret)
		return ret;

	ret = clk_enable(&priv->clk);
	if (ret)
		return ret;

	return 0;
}

static const struct pwm_ops sunxi_pwmcs_ops = {
	.set_invert	= sunxi_pwmcs_set_invert,
	.set_config	= sunxi_pwmcs_set_config,
	.set_enable	= sunxi_pwmcs_set_enable,
};

static const struct udevice_id sunxi_pwmcs_ids[] = {
	{ .compatible = "allwinner,sunxi-pwmcs", },
	{ }
};

U_BOOT_DRIVER(sunxi_pwmcs) = {
	.name	= "sunxi_pwmcs",
	.id	= UCLASS_PWM,
	.of_match = sunxi_pwmcs_ids,
	.ops	= &sunxi_pwmcs_ops,
	.of_to_plat	= sunxi_pwmcs_of_to_plat,
	.probe		= sunxi_pwmcs_probe,
	.plat_auto	= sizeof(struct sunxi_pwmcs_priv),
};
