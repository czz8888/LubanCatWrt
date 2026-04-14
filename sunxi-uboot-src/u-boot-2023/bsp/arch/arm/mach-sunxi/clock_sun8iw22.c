/*
 * Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 *
 * SPDX-License-Identifier:	GPL-2.0+
*/

#include <common.h>
#include <asm/io.h>
#include <linux/delay.h>
#include <asm/arch/cpu.h>
#include <asm/arch/clock.h>
#include <asm/arch/timer.h>
#include <asm/arch/prcm.h>
#include "private_uboot.h"

#define CPU_UPDATE_OFFSET      (26)
#define CPU_LOCK_OFFSET	       (28)
#define CPU_LOCK_ENABLE_OFFSET (29)
static void line_pll_switch_freq(phys_addr_t reg_addr, u32 n_factor)
{
	u32 reg_val;
	uint judge_cnt = 0;

	//--cfg pll
	//set n=0x2a,m0=m1=1,p=1,cpu pll 1008 Mhz
	//set n=0x26,m0=m1=1,p=1,cpu pll 912 Mhz
	//24M*n/p/(m0 * m1)
	reg_val	= readl(reg_addr);
	reg_val &= ~((0xffU << 8) | (0x7U << 16) | (0x3U << 20) | (0xfU << 0));
	reg_val |= (n_factor << 8) | (0x0 << 0);
	writel(reg_val, reg_addr);
	udelay(10);

	//update_bit
	reg_val = readl(reg_addr);
	reg_val |= (0x1U << CPU_UPDATE_OFFSET);
	writel(reg_val, reg_addr);
	do {
		reg_val = readl(reg_addr);
		reg_val &= (0x1U << CPU_UPDATE_OFFSET);
		// hardware clear to 0, bit26 must be 0 before use
	} while (reg_val);

	//lock enable
	reg_val	= readl(reg_addr);
	reg_val &= ~(0x1U << CPU_LOCK_ENABLE_OFFSET);
	writel(reg_val, reg_addr);
	udelay(10);
	reg_val |= (0x1U << CPU_LOCK_ENABLE_OFFSET);
	writel(reg_val, reg_addr);

	//wait lock
	do {
		udelay(3);
		reg_val = readl(reg_addr);
		reg_val &= (0x1U << CPU_LOCK_OFFSET);
		judge_cnt = (reg_val) ? judge_cnt + 1 : 0;
		//must judge 3 times continuously, then regard lock status as stable
	} while (judge_cnt < 3);

	udelay(20);
}

static void set_pll_cpux_axi(u32 n_factor)
{
	u32 reg_val;

	line_pll_switch_freq(CCMU_PLL_CPU_CTRL_REG, n_factor);

	/* set cpu_div factor P */
	reg_val = readl(CLU_CLK_DIV_CFG_REG);
	reg_val &= ~(0x3 << 16);

	/* set cpu_axi_div factor M */
	reg_val &= ~(0x3 << 0);
	reg_val |= (0x1 << 0);

	writel(reg_val, CLU_CLK_DIV_CFG_REG);
	udelay(10);

	/* set cpu clock source */
	reg_val = readl(CLU_CLK_REG);
	reg_val &= ~(0x7 << 24);
	reg_val |= (0x3 << 24);

	writel(reg_val, CLU_CLK_REG);
	udelay(10);
}

void clock_open_timer(int timernum)
{
	u32 reg_value = 0;
	reg_value = readl(SUNXI_CCM_BASE + TIMER0_0_RV_CLK_REG + timernum * 4);
	reg_value |= (0 <<24);
	reg_value |= (2 << 0);
	writel(reg_value, (SUNXI_CCM_BASE + TIMER0_0_RV_CLK_REG + timernum * 4));

	/*enable timer*/
	reg_value = readl(SUNXI_CCM_BASE + TIMER0_0_RV_CLK_REG + timernum * 4);
	reg_value |= (1 << 31);
	writel(reg_value, (SUNXI_CCM_BASE + TIMER0_0_RV_CLK_REG + timernum * 4));

	reg_value = readl(SUNXI_CCM_BASE + TIMER0_RV_GAR_REG);
	reg_value |= (1 << 16);
	writel(reg_value, (SUNXI_CCM_BASE + TIMER0_RV_GAR_REG));

	reg_value = readl(SUNXI_CCM_BASE + TIMER0_RV_GAR_REG);
	reg_value |= (1 << 0);
	writel(reg_value, (SUNXI_CCM_BASE + TIMER0_RV_GAR_REG));
	__msdelay(1);

}

void clock_close_timer(int timernum)
{
	u32 reg_value = 0;
	/*disable timer*/
	reg_value = readl(SUNXI_CCM_BASE + TIMER0_0_RV_CLK_REG + timernum * 4);
	reg_value |= (0 << 31);
	writel(reg_value, (SUNXI_CCM_BASE + TIMER0_0_RV_CLK_REG + timernum * 4));

	reg_value = readl(SUNXI_CCM_BASE + TIMER0_RV_GAR_REG);
	reg_value &= ~(1 << 0);
	writel(reg_value, (SUNXI_CCM_BASE + TIMER0_RV_GAR_REG));
	__msdelay(1);
}

int clock_gate_rate(int timernum)
{
	u32 reg_value, factor_p_val, clk_src_val, clk_rate, i;
	u32 factor_p = 1;

	reg_value = readl(SUNXI_CCM_BASE + TIMER0_0_RV_CLK_REG + timernum * 4);
	factor_p_val = (reg_value >> TIMER0_0_RV_CLK_REG_FACTOR_M_OFFSET) & 0x3;
	for (i = 0; i < factor_p_val; i++) {
		factor_p *= 2;
	}
	clk_src_val = (reg_value >> TIMER0_0_RV_CLK_REG_CLK_SRC_SEL_OFFSET) & 0x7;

	switch (clk_src_val) {
	case 0:
		clk_rate = 24000000 / factor_p;
		break;
	case 1:
		clk_rate = 16000000 / factor_p;
		break;
	case 2:
		clk_rate = 32000 / factor_p;
		break;
	case 3:
		clk_rate = 200000000 / factor_p;
		break;
	default:
		clk_rate = 0;
		break;
	}

	return clk_rate;
}

int usb_open_clock(void)
{
	u32 reg_val;

	/* USB2P0_SYS Gating And Reset */
	clrbits_le32(SUNXI_CCM_BASE + USB2P0_SYS_GAR_REG,
		     BIT(USB2P0_SYS_GAR_REG_USB2P0_SYS_RST_N_OFFSET));
	__msdelay(1);
	setbits_le32(SUNXI_CCM_BASE + USB2P0_SYS_GAR_REG,
		     BIT(USB2P0_SYS_GAR_REG_USB2P0_SYS_RST_N_OFFSET));
	__msdelay(1);

	setbits_le32(SUNXI_CCM_BASE + USB2P0_SYS_GAR_REG,
		     BIT(USB2P0_SYS_GAR_REG_USB2P0_SYS_AHB_CLK_EN_OFFSET));
	__msdelay(1);

	/* USB0 Gating And Reset */
	clrbits_le32(SUNXI_CCM_BASE + USB0_GAR_REG,
		     BIT(USB0_GAR_REG_USB0_DEV_RST_N_OFFSET));
	__msdelay(1);
	setbits_le32(SUNXI_CCM_BASE + USB0_GAR_REG,
		     BIT(USB0_GAR_REG_USB0_DEV_RST_N_OFFSET));
	__msdelay(1);

	setbits_le32(SUNXI_CCM_BASE + USB0_GAR_REG,
		     BIT(USB0_GAR_REG_USB0_DEV_AHB_CLK_EN_OFFSET));
	__msdelay(1);

	reg_val = readl(SUNXI_USBOTG_BASE + 0x420);
	reg_val |= (0x01 << 0);
	writel(reg_val, SUNXI_USBOTG_BASE + 0x420);

	/* enable usb0 phy */
	reg_val = readl(SUNXI_USB_PHY_CTRL);
	reg_val &= ~(0x01 << 3);
	writel(reg_val, SUNXI_USB_PHY_CTRL);

	/* deassert phy reset */
	reg_val = readl(SUNXI_USB_RST_CTRL);
	reg_val |= (0x01 << 0);
	writel(reg_val, SUNXI_USB_RST_CTRL);

	return 0;
}


int usb_close_clock(void)
{
	u32 reg_val;

	/* disable usb0 phy */
	reg_val = readl(SUNXI_USB_PHY_CTRL);
	reg_val |= (0x01 << 3);
	writel(reg_val, SUNXI_USB_PHY_CTRL);

	/* assert phy reset */
	reg_val = readl(SUNXI_USB_RST_CTRL);
	reg_val &= ~(0x01 << 0);
	writel(reg_val, SUNXI_USB_RST_CTRL);

	clrbits_le32(SUNXI_CCM_BASE + USB2P0_SYS_GAR_REG,
		     BIT(USB2P0_SYS_GAR_REG_USB2P0_SYS_AHB_CLK_EN_OFFSET) |
			     BIT(USB2P0_SYS_GAR_REG_USB2P0_SYS_RST_N_OFFSET));
	__msdelay(1);

	clrbits_le32(SUNXI_CCM_BASE + USB0_GAR_REG,
		     BIT(USB0_GAR_REG_USB0_DEV_AHB_CLK_EN_OFFSET) |
			     BIT(USB0_GAR_REG_USB0_DEV_RST_N_OFFSET));
	__msdelay(1);

	return 0;
}

uint clock_get_corepll(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	unsigned int reg_val;
	int 	div_m0, div_m1, div_p;
	int 	factor_n;
	int 	clock, clock_src;

	reg_val   = readl(CLU_CLK_REG);
	clock_src = (reg_val >> 24) & 0x7;

	switch (clock_src) {
	case 0://OSC24M
		clock = 24;
		break;
	case 1://RTC32K
		clock = 32 / 1000 ;
		break;
	case 2://RC16M
		clock = 16;
		break;
	case 3://PLL_CPUX
		reg_val  = readl(&ccm->pll1_cfg);
		div_p	 = ((reg_val >> 16) & 0x7) + 1;
		factor_n = ((reg_val >> 8) & 0xff);
		div_m1   = ((reg_val >> 0) & 0xf) + 1;
		div_m0   = ((reg_val >> 20) & 0x3) + 1;
		clock = (24 * factor_n / div_p / div_m1 / div_m0);
		break;
	case 4://PERIPLL2X
		clock = clock_get_pll6() << 1;
		break;
	case 5://PERIPLL1X
		clock = clock_get_pll6();
		break;
	default:
		return 0;
	}
	return clock;
}

uint clock_set_corepll(int frequency)
{
	u32 pll_factor;

	if (clock_get_corepll() == frequency)
		return 0;

	pll_factor = frequency / 24;
	set_pll_cpux_axi(pll_factor);

	return 0;
}

uint clock_get_pll6(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	uint reg_val;
	uint factor_n, factor_p0, factor_m1, pll6;

	reg_val = readl(&ccm->pll6_cfg);

	factor_n = ((reg_val >> 8) & 0xff) + 1;
	factor_p0 = ((reg_val >> 16) & 0x3) + 1;
	factor_m1 = ((reg_val >> 1) & 0x1) + 1;
	pll6 = (24 * factor_n / factor_p0 / factor_m1) >> 1;

	return pll6;
}

uint clock_get_ahb(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	uint reg_val;
	uint factor_m;
	uint clock, clock_src;

	reg_val = readl(&ccm->psi_ahb1_ahb2_cfg);
	clock_src = (reg_val >> 24) & 0x3;
	factor_m  = ((reg_val >> 0) & 0xf) + 1;

	switch (clock_src) {
	case 0://OSC24M
		clock_src = 24;
		break;
	case 1://CCMU_32K
		clock_src = 32 / 1000;
		break;
	case 2:	//RC16M
		clock_src = 16;
		break;
	case 3://PERIPLL1X
		clock_src = clock_get_pll6();
		break;
	default:
		return 0;
	}

	clock = clock_src / factor_m;

	return clock;
}

uint clock_get_apb1(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	uint reg_val;
	uint factor_m;
	uint clock, clock_src;

	reg_val = readl(&ccm->apb1_cfg);
	clock_src = (reg_val >> 24) & 0x3;
	factor_m  = ((reg_val >> 0) & 0xf) + 1;

	switch (clock_src) {
	case 0://OSC24M
		clock_src = 24;
		break;
	case 1://CCMU_32K
		clock_src = 32 / 1000;
		break;
	case 2:	//RC16M
		clock_src = 16;
		break;
	case 3://PERIPLL1X
		clock_src = clock_get_pll6();
		break;
	default:
		return 0;
	}

	clock = clock_src / factor_m;

	return clock;
}

uint clock_get_pll_ddr(void)
{
	uint reg_val;
	uint factor_n;
	uint factor_m = 1;
	uint clock, clock_src;

	reg_val   = readl(DRAM_CLK_CTRL_REG);
	clock_src = (reg_val >> 8) & 0x3;

	reg_val   = readl(SSC_CTRL1_REG_P(clock_src));
	factor_n  = (reg_val >> 4) & 0xff;

	clock = 24 * factor_n / factor_m;

	return clock;
}

uint clock_get_mbus(void)
{
	struct sunxi_ccm_reg *const ccm =
		(struct sunxi_ccm_reg *)SUNXI_CCM_BASE;
	uint reg_val;
	uint factor_m;
	uint clock, clock_src;

	reg_val = readl(&ccm->mbus_cfg);
	clock_src = (reg_val >> 24) & 0x7;
	factor_m  = ((reg_val >> 0) & 0xf) + 1;

	switch (clock_src) {
	case 0://OSC24M
		clock_src = 24;
		break;
	case 1://PERIPLL480M
		clock_src = 480;
		break;
	case 2:	//PERIPLL400M
		clock_src = 400;
		break;
	case 3://PERIPLL300M
		clock_src = 300;
		break;
	case 4://HDR_CLK
		clock_src = clock_get_pll_ddr() >> 2;
		break;
	default:
		return 0;
	}

	clock = clock_src / factor_m;

	return clock;
}