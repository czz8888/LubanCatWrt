/* SPDX-License-Identifier: GPL-2.0+ */
#include <asm/arch/plat-sun8iw22p1/cpu_autogen.h>

#define SUNXI_GIC400_BASE          SUNXI_CPU_GIC400_BASE
#define SUNXI_PIO_BASE             SUNXI_GPIO_BASE
#define SUNXI_RTC_DATA_BASE        (SUNXI_RTC_BASE+0x100)
#define SUNXI_R_PIO_BASE           SUNXI_R_GPIO_BASE
#define SUNXI_MMC0_BASE            SUNXI_SMHC0_BASE
#define SUNXI_PIOC_REG_POW_VAL     0x48
#define PIOC_VAL_Px_1_8V_VOL       0x0
#define PIOC_REG_POW_VAL           (SUNXI_PIO_BASE + SUNXI_PIOC_REG_POW_VAL)
#define SUNXI_DMA_BASE             (SUNXI_DMAC0_BASE)
#define SUNXI_SS_BASE		   SUNXI_CE_BASE

/* usb */
#define SUNXI_USBOTG_BASE          (SUNXI_USB0_BASE)
#define SUNXI_USB_PHY_CTRL         (SUNXI_USB2P0_PHY_BASE + 0x10)
#define SUNXI_USB_RST_CTRL         (SUNXI_USB2P0_PHY_BASE + 0x28)
#define SUNXI_USB2P0_MAC_MAP       (SUNXI_USB2P0_SYS_DIG_BASE)
#define SUNXI_USB2P0_PHY_MAP       (SUNXI_USB2P0_SYS_DIG_BASE + 0x4)
#define SUNXI_USB_DCTRL            (SUNXI_USB2P0_SYS_DIG_BASE + 0x8)
