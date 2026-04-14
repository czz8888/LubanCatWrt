// SPDX-License-Identifier: GPL-2.0+
/*
 * sunxi dual storage.
 *
 * Copyright (C) 2024
 * 2024.12.27 linzibo <linzibo@allwinnertech.com>
 */

#include <common.h>
#include <sunxi_board.h>
#include <sunxi_dual_storage.h>
#include <fdt_support.h>
#include <sys_config.h>

extern int sunxi_flash_boot_init(int storage_type, int workmode);
extern int sunxi_flash_sprite_init(int storage_type, int workmode);
extern int sunxi_flash_sprite_switch(int storage_type, int workmode);

int sunxi_fdt_get_boot_storage_type(const void *fdt)
{
	return fdt_getprop_u32_default(fdt, FDT_PATH_TARGET, "boot_storage_type", -1);
}

int sunxi_fdt_get_system_storage_type(const void *fdt)
{
	return fdt_getprop_u32_default(fdt, FDT_PATH_TARGET, "system_storage_type", -1);
}

int sunxi_dual_storage_handle(int stage, int workmode)
{
	int ret = 0;
	int storage_type;

	switch (stage) {
	case SUNXI_DUAL_STORAGE_BOOT: {
		storage_type = sunxi_fdt_get_system_storage_type(working_fdt);
		if (storage_type < 0) {
			pr_err("FDT ERROR:%s:get property system_storage_type error\n", __func__);
			return -1;
		}
		pr_info("system storage type = %d\n", storage_type);
		ret = sunxi_flash_boot_init(storage_type, workmode);
	} break;
	case SUNXI_DUAL_STORAGE_SPRITE: {
		storage_type = sunxi_fdt_get_boot_storage_type(working_fdt);
		if (storage_type < 0) {
			pr_err("FDT ERROR:%s:get property boot_storage_type error\n", __func__);
			return -1;
		}
		pr_info("boot storage type = %d\n", storage_type);
		ret = sunxi_flash_sprite_init(storage_type, workmode);
		if (ret < 0) {
			pr_err("%s:init boot storage fail\n", __func__);
			return -1;
		}
		storage_type = sunxi_fdt_get_system_storage_type(working_fdt);
		if (storage_type < 0) {
			pr_err("FDT ERROR:%s:get property system_storage_type error\n", __func__);
			return -1;
		}
		pr_info("system storage type = %d\n", storage_type);
		ret = sunxi_flash_sprite_init(storage_type, workmode);
	} break;
	case SUNXI_DUAL_STORAGE_SWITCH: {
		storage_type = sunxi_fdt_get_boot_storage_type(working_fdt);
		if (storage_type < 0) {
			pr_err("FDT ERROR:%s:get property boot_storage_type error\n", __func__);
			return -1;
		}
		pr_info("boot storage type = %d\n", storage_type);
		ret = sunxi_flash_sprite_switch(storage_type, workmode);
	} break;
	default: {
		pr_err("not support\n");
		return -1;
	} break;
	}

	if (ret != 0) {
		pr_err("dual storage handle fail, stage = %d, workmode = %d\n", stage, workmode);
		return -1;
	}
	set_boot_storage_type(storage_type);

	return 0;
}
