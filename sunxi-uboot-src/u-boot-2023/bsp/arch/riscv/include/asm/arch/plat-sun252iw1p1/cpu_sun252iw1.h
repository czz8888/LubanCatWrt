/* SPDX-License-Identifier: GPL-2.0+ */
#include <asm/arch/plat-sun252iw1p1/cpu_autogen.h>
#define SUNXI_PIO_BASE                (SUNXI_GPIO_BASE)
#define SUNXI_RTC_DATA_BASE           (SUNXI_RTC_BASE+0x100)
/*#define SUNXI_RSB_BASE*/
#define SUNXI_CCM_BASE                (SUNXI_CCMU_BASE)
#define SUNXI_EHCI1_BASE              (SUNXI_USB0_BASE + 0x1000)
#define SUNXI_USBOTG_BASE             (SUNXI_USB0_BASE)
#define SUNXI_DMA_BASE                (SUNXI_DMAC0_BASE)
#define SUNXI_MMC0_BASE               (SUNXI_SMHC0_BASE)
#define SUNXI_MMC1_BASE               (SUNXI_SMHC1_BASE)
#define SUNXI_MMC2_BASE               (SUNXI_SMHC2_BASE)
#define SUNXI_PRCM_BASE               (SUNXI_R_PRCM_BASE)
#define SUNXI_NFC_BASE                (SUNXI_NAND_BASE)
#define SUNXI_SS_BASE                 (SUNXI_CE_NS_BASE)

#define PIOC_VAL_Px_1_8V_VOL 0
#define PIOC_VAL_Px_3_3V_VOL 1

#define PIOC_REG_o_POW_MS_VAL 0x48
#define PIOC_REG_POW_VAL (IOMEM_ADDR(SUNXI_PIO_BASE + PIOC_REG_o_POW_MS_VAL))

#define SUNXI_PLIC_BASE                 (SUNXI_CPUX_PLIC_BASE)
#define SUNXI_WDT_BASE			(SUNXI_CPUX_WDG_BASE)
