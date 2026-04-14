/* SPDX-License-Identifier: GPL-2.0+ */
#include <asm/arch/plat-sun8iw22p1/clock_autogen.h>
#define  sunxi_ccm_reg           CCU_st
#define  pll1_cfg                pll_cpu_ctrl_reg
#define  pll6_cfg                pll_peri0_ctrl_reg
#define  psi_ahb1_ahb2_cfg       ahb_clk_reg
#define  apb1_cfg                apb0_clk_reg
#define  mbus_cfg                mbus_clk_reg

#define CCM_MMC_CTRL_ENABLE             (SMHC0_CLK_REG_SMHC0_CLK_GATING_CLOCK_IS_ON << SMHC0_CLK_REG_SMHC0_CLK_GATING_OFFSET)
#define SUNXI_CCM_BASE                  (SUNXI_CCMU_BASE)

/* MMC clock bit field */
#define CCM_MMC_CTRL_M(x)               (x)
#define CCM_MMC_CTRL_N(x)               ((x) << SMHC0_CLK_REG_FACTOR_N_OFFSET)
#define CCM_MMC_CTRL_OSCM24             (SMHC0_CLK_REG_CLK_SRC_SEL_HOSC << SMHC0_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_PLL6X2             (SMHC0_CLK_REG_CLK_SRC_SEL_PERI0_400M << SMHC0_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_PLL_PERIPH2X2      (SMHC0_CLK_REG_CLK_SRC_SEL_PERI0_300M << SMHC0_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_PERI0_400M_freq         (400000000)
#define CCM_MMC_CTRL_PERI0_300M_freq         (300000000)
#define CCM_MMC_CTRL_PERI0_800M_freq         (800000000)
#define CCM_MMC_CTRL_PERI0_600M_freq         (600000000)
#define CCM_MMC_CTRL_PERI0_400M         (0x1 << 24)
#define CCM_MMC_CTRL_PERI0_300M         (0x2 << 24)
#define CCM_MMC_CTRL_PERI0_800M         (0x1 << 24)
#define CCM_MMC_CTRL_PERI0_600M         (0x2 << 24)
#define CCM_MMC_CTRL_PERI1_400M		(0x3 << SMHC0_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_PERI1_300M		(0x4 << SMHC0_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_PERI1_800M		(0x3 << SMHC2_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_PERI1_600M		(0x4 << SMHC2_CLK_REG_CLK_SRC_SEL_OFFSET)
#define CCM_MMC_CTRL_ENABLE             (SMHC0_CLK_REG_SMHC0_CLK_GATING_CLOCK_IS_ON << SMHC0_CLK_REG_SMHC0_CLK_GATING_OFFSET)

/* cpux cfg */
#define CLU_CLK_REG 			(SUNXI_CPUX_PLL_CFG_BASE + 0x18)
#define CLU_CLK_DIV_CFG_REG		(SUNXI_CPUX_PLL_CFG_BASE + 0x20)
#define CCMU_PLL_CPU_CTRL_REG	(SUNXI_CCM_BASE + PLL_CPU_CTRL_REG)
/* ddr cfg */
#define DRAM_CLK_CTRL_REG		(SUNXI_MEMC_PHY_BASE + 0x1c)
#define SSC_CTRL1_REG_P(n)		(SUNXI_MEMC_PHY_BASE + 0x4 + 0x200 * n)

/* UART */
#define SUNXI_UART_CLK_REG		(SUNXI_CCM_BASE + APB_UART_CLK_REG)
#define SUNXI_UART_CLK_SRC_SEL_HOSC		0x0
#define SUNXI_UART_CLK_SRC_PERI0_600M	0x3
#define SUNXI_UART_CLK_SRC_PERI0_480M	0x4