/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sunxi's rproc boot reason plat def
 * some platform definitions of boot reason for rproc.
 *
 * Copyright (C) 2023 Allwinnertech - All Rights Reserved
 *
 * Author: shihongfu <shihongfu@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SUNXI_BOOT_REASON_PLAT_H__
#define __SUNXI_BOOT_REASON_PLAT_H__

#include <sunxi_board.h>
#include <asm/io.h>
#include "sunxi_boot_reason.h" /* Public API */
#include "chip_sun8iw22p1.h" /* chip def, check it first */

#define ROLE	WRITER_UBOOT

#define br_print		printf
#ifdef RPROC_DEBUG
#define br_trace		printf
#else
#define br_trace(...)		do { } while (0)
#endif

#endif /* __SUNXI_BOOT_REASON_PLAT_H__ */