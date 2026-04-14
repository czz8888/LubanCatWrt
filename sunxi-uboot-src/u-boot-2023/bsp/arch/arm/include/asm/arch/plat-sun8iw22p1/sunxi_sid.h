//SPDX-License-Identifier:     GPL-2.0+

#ifndef __SUNXI_SID_H__
#define __SUNXI_SID_H__

#include <linux/types.h>
#include <asm/arch/cpu.h>

/* efuse power ctrl */
#define EFUSE_HV_SWITCH			(SUNXI_RTC_BASE + 0x204)

/* sid regiseter */
#define SID_PRCTL			(SUNXI_SID_BASE + 0x40)
#define SID_PRKEY			(SUNXI_SID_BASE + 0x50)
#define SID_RDKEY			(SUNXI_SID_BASE + 0x60)

#define SJTAG_S				(SUNXI_SID_BASE + 0x88)

#define SID_ROTPK_VALUE(n)		(SUNXI_SID_BASE + 0x120 + (n * 4))
#define SID_ROTPK_CTRL			(SUNXI_SID_BASE + 0x140)
#define SID_ROTPK_EFUSED_BIT		(1)
#define SID_ROTPK_CMP_RET_BIT		(0)

#define SID_EFUSE			(SUNXI_SID_BASE + 0x200)
#define SID_SECURE_MODE			(SUNXI_SID_BASE + 0xA0)
#define SID_OP_LOCK			(0xAC)

/* efuse mapping */
#define EFUSE_CHIPID			(0x0)
#define SID_CHIPID_SIZE			(128)
#define SCC_CHIPID_BURNED_FLAG		(0)

#define EFUSE_OEM_PROGRAM		(0x44)
#define SID_OEM_PROGRAM_SIZE		(96)

#define EFUSE_ROTPK			(0x80)
#define SID_ROTPK_SIZE			(256)
#define SCC_ROTPK_BURNED_FLAG		(14)

#define EFUSE_SSK			(0xA0)
#define SID_SSK_SIZE			(256)
#define SCC_SSK_BURNED_FLAG		(15)

#define EFUSE_WRITE_PROTECT		(0x50)
#define EFUSE_READ_PROTECT		(0x54)
#define EFUSE_LCJS			(0x58)

#define EFUSE_OEM_PROGRAM_SECURE	(0xD8)
#define SID_OEM_PROGRAM_SECURE_SIZE	(320)

#define EFUSE_BURN_RD_OFFSET_MAX 	(20)

#define SECURE_BIT_OFFSET		(0)

#define EFUSE_CONFIG			(0x10)
#define FEL_VERIFY_OFFSET		(11)

/* efuse read and write protect configuration info */
#define EFUSE_ACL_SET_BURN_BIT		(0)
#define EFUSE_ACL_SET_RD_FORBID_BIT	(0)
#define EFUSE_BURN_RD_OFFSET_MASK	(0xFFFFFFFF)

#endif    /*  #ifndef __SUNXI_SID_H__  */
