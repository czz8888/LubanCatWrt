/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 */
#ifndef	_ASM_ARCH_SPL_H_
#define	_ASM_ARCH_SPL_H_

#define BOOT0_MAGIC		"eGON.BT0"
#define SPL_SIGNATURE		"SPL" /* marks "sunxi" SPL header */

#ifdef CONFIG_SUNXI_HIGH_SRAM
#define SPL_ADDR		0x10000
#else
#define SPL_ADDR		0x0
#endif

/* The low 8-bits of the 'boot_media' field in the SPL header */
#define SUNXI_BOOTED_FROM_MMC0	0
#define SUNXI_BOOTED_FROM_NAND	1
#define SUNXI_BOOTED_FROM_MMC2	2
#define SUNXI_BOOTED_FROM_SPI	3

/* boot head definition from sun4i boot code */

/* Compile time check to assure proper alignment of structure */

#define is_boot0_magic(addr)	(memcmp((void *)addr, BOOT0_MAGIC, 8) == 0)

uint32_t sunxi_get_boot_device(void);

#endif
