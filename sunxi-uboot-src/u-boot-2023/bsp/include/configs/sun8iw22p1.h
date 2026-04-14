/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Configuration settings for the Allwinner A7 (sun8i) CPU
 */

#ifndef __CONFIG_H
#define __CONFIG_H

#ifdef CONFIG_USB_EHCI_HCD
#define CONFIG_USB_EHCI_SUNXI
#define CONFIG_USB_MAX_CONTROLLER_COUNT 1
#endif

#define CONFIG_SUNXI_USB_PHYS	1

#define AW_IRQ_USB_OTG                 61
#define AW_IRQ_USB_EHCI0               62
#define AW_IRQ_USB_OHCI0               63
#define AW_IRQ_DMA                     82
#define AW_IRQ_TIMER0                  218
#define AW_IRQ_TIMER1                  219
#define AW_IRQ_NMI                     224
#define GIC_IRQ_NUM                    351

/* sram layout*/

#define SUNXI_SRAM_A1_BASE		(0x40000L)
#define SUNXI_SRAM_A1_SIZE		(0x20000)

#define SUNXI_SYS_SRAM_BASE		SUNXI_SRAM_A1_BASE
#define SUNXI_SYS_SRAM_SIZE		(SUNXI_SRAM_A1_SIZE)

#ifdef CONFIG_SUNXI_MALLOC_LEN
#define SUNXI_SYS_MALLOC_LEN	CONFIG_SUNXI_MALLOC_LEN
#else
#define SUNXI_SYS_MALLOC_LEN	(32 << 20)
#endif

#define PHOENIX_PRIV_DATA_ADDR	(0x68000)
/*
 * Include common sunxi configuration where most the settings are
 */
#include <configs/sunxi-common.h>

#endif /* __CONFIG_H */
