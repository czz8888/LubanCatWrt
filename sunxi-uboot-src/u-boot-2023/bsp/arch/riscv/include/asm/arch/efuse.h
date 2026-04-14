/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2018
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * wangwei <wangwei@allwinnertech.com>
 */

#ifndef _EFUSE_H_
#define _EFUSE_H_

#include <linux/types.h>


#if defined(CONFIG_MACH_SUN252IW1)
#include <asm/arch/plat-sun252iw1p1/sunxi_sid.h>
#else
#error "platform not support"
#endif

#ifndef __ASSEMBLY__
#if defined(CONFIG_DM)
int sunxi_efuse_read_dm(void *key_name, void *rd_buf, int *len);
int sunxi_efuse_write_dm(void *key_info);
int sid_probe_security_mode_dm(void);
int sunxi_efuse_get_rotpk_status_dm(void);
int sid_set_security_mode_dm(void);
int sid_get_security_status_dm(void);
int sunxi_efuse_verify_rotpk_dm(u8 *hash);
int sunxi_efuse_usr_write(void *key_buf);
int sunxi_efuse_usr_read(void *key_name, void *read_buf, int *len);
#else
int sunxi_efuse_get_rotpk_status(void);
int sid_probe_security_mode(void);
int sid_set_security_mode(void);
int  sid_get_security_status(void);
int sunxi_efuse_verify_rotpk(u8 *hash);
#endif /* CONFIG_DM */
#endif /* __ASSEMBLY__ */

#endif /* _EFUSE_H_ */
