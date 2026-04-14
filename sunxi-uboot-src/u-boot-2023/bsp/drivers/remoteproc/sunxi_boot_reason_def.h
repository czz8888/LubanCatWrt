/* SPDX-License-Identifier: GPL-2.0 */
/*
 * sunxi's rproc boot reason def
 * some definitions of boot reason for rproc.
 *
 * Copyright (C) 2023 Allwinnertech - All Rights Reserved
 *
 * Author: shihongfu <shihongfu@allwinnertech.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __SUNXI_BOOT_REASON_DEF_H__
#define __SUNXI_BOOT_REASON_DEF_H__

#define CHECK_BITS	(0b11111111)

#define CHECK_SRC_BITS	(CHECK_BITS)
#define CHECK_SRC_OFF	(0)
#define CHECK_DST_BITS	(CHECK_BITS)
#define CHECK_DST_OFF	(24)

#define REASON_BIT_NUM	(8)

#define REASON_BITS	(0b1111)
#define REASON_OFF	(0)

#define WRITER_BITS	(0b11)
#define WRITER_OFF	(6)

enum writer_t {
	WRITER_BOOT0 = 0x0,
	WRITER_UBOOT = 0x1,
	WRITER_LINUX = 0x2,
	WRITER_MCU   = 0x3,
};

enum boot_reason_t {
	BOOT_REASON_INVALID_RST = 0x0, /* boot0/uboot */
	BOOT_REASON_SYS_COLD_RST = 0x1, /* boot0/uboot */
	BOOT_REASON_SYS_HARD_RST = 0x2, /* boot0/uboot */
	BOOT_REASON_VCC_DET_RST = 0x3, /* boot0/uboot */
	BOOT_REASON_VDD_DET_RST = 0x4, /* boot0/uboot */
	BOOT_REASON_SYS_WDT_RST	= 0x5, /* boot0/uboot */
	BOOT_REASON_TWD_WDT_RST = 0x6, /* boot0/uboot */
	BOOT_REASON_MCU_WDT_RST	= 0x7, /* boot0/uboot */

	BOOT_REASON_RESERVED0 = 0x8,
	BOOT_REASON_RESERVED1 = 0x9,
	BOOT_REASON_MCU_USER0 = 0xa, /* mcu */
	BOOT_REASON_MCU_USER1 = 0xb, /* mcu */
	BOOT_REASON_MCU_PANIC = 0xc, /* mcu */

	BOOT_REASON_MCU_WDT_TO = 0xd, /* linux */
	BOOT_REASON_USER_STOP = 0xe, /* linux */

	BOOT_REASON_MCU_CLEAR = 0xf, /* mcu */
};

static inline
int boot_reason_priority(enum boot_reason_t reason)
{
	switch (reason) {
	case BOOT_REASON_SYS_COLD_RST:
	case BOOT_REASON_SYS_HARD_RST:
	case BOOT_REASON_VCC_DET_RST:
	case BOOT_REASON_VDD_DET_RST:
	case BOOT_REASON_SYS_WDT_RST:
	case BOOT_REASON_TWD_WDT_RST:
	case BOOT_REASON_MCU_WDT_RST:
		return 6;
	case BOOT_REASON_RESERVED0:
	case BOOT_REASON_RESERVED1:
	case BOOT_REASON_MCU_USER0:
	case BOOT_REASON_MCU_USER1:
	case BOOT_REASON_MCU_PANIC:
		return 4;
	case BOOT_REASON_MCU_WDT_TO:
	case BOOT_REASON_USER_STOP:
		return 2;
	case BOOT_REASON_MCU_CLEAR:
	case BOOT_REASON_INVALID_RST:
	default:
		return 0;
	}
}

static inline
const char *boot_reason_str(enum boot_reason_t reason)
{
	switch (reason) {
	case BOOT_REASON_INVALID_RST: return "BOOT_REASON_INVALID_RST";
	case BOOT_REASON_SYS_COLD_RST: return "BOOT_REASON_SYS_COLD_RST";
	case BOOT_REASON_SYS_HARD_RST: return "BOOT_REASON_SYS_HARD_RST";
	case BOOT_REASON_VCC_DET_RST: return "BOOT_REASON_VCC_DET_RST";
	case BOOT_REASON_VDD_DET_RST: return "BOOT_REASON_VDD_DET_RST";
	case BOOT_REASON_SYS_WDT_RST: return "BOOT_REASON_SYS_WDT_RST";
	case BOOT_REASON_TWD_WDT_RST: return "BOOT_REASON_TWD_WDT_RST";
	case BOOT_REASON_MCU_WDT_RST: return "BOOT_REASON_MCU_WDT_RST";

	case BOOT_REASON_RESERVED0: return "BOOT_REASON_RESERVED0";
	case BOOT_REASON_RESERVED1: return "BOOT_REASON_RESERVED1";
	case BOOT_REASON_MCU_USER0: return "BOOT_REASON_MCU_USER0";
	case BOOT_REASON_MCU_USER1: return "BOOT_REASON_MCU_USER1";
	case BOOT_REASON_MCU_PANIC: return "BOOT_REASON_MCU_PANIC";

	case BOOT_REASON_MCU_WDT_TO: return "BOOT_REASON_MCU_WDT_TO";
	case BOOT_REASON_USER_STOP: return "BOOT_REASON_USER_STOP";

	case BOOT_REASON_MCU_CLEAR: return "BOOT_REASON_MCU_CLEAR";
	default:                    return "unknown";
	}
}

#define _CONTACT(__STR_X, __STR_Y)		__STR_X##__STR_Y
#define CONTACT(_STR_X, _STR_Y)			_CONTACT(_STR_X, _STR_Y)

#define RBT_REG_OFFSET(_name)			CONTACT(_name, _OFFSET)
#define RBT_REG_ADDR(_base, _name)		(((void __iomem *)_base) + RBT_REG_OFFSET(_name))
#define RBT_OFF(_name)				CONTACT(_name, _OFF)
#define RBT_BITS(_name)				CONTACT(_name, _BITS)
#define RBT_MASK(_name)				((RBT_BITS(_name))<<(RBT_OFF(_name)))
#define RBT_VAL(_reg_val, _name)		(((_reg_val)>>RBT_OFF(_name))&RBT_BITS(_name))
#define RBT_NVAL(_reg_val, _name)		(((~_reg_val)>>RBT_OFF(_name))&RBT_BITS(_name))

static inline
unsigned int boot_reason_to_reg(unsigned int last_val, unsigned int role, enum boot_reason_t reason)
{
	unsigned int val = (last_val << REASON_BIT_NUM) & ~RBT_MASK(CHECK_DST);

	val |= (role & WRITER_BITS) << WRITER_OFF;
	val |= (reason & REASON_BITS) << REASON_OFF;
	val |= ((RBT_NVAL(val, CHECK_SRC)) & CHECK_DST_BITS) << CHECK_DST_OFF;

	return val;
}

static inline
enum boot_reason_t reg_to_boot_reason(unsigned int val)
{
	if (RBT_VAL(val, CHECK_SRC) != RBT_NVAL(val, CHECK_DST))
		return BOOT_REASON_INVALID_RST;

	return RBT_VAL(val, REASON);
}

#endif /* __SUNXI_BOOT_REASON_DEF_H__ */