// z:\workspace\workspace\tina-v861\brandy\brandy-2.0\u-boot-bsp\drivers\power\include\sunxi_supply.h
// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 AXP Supply Driver
 * Optimized supply framework consistent with regulator design
 */

#ifndef _SUNXI_INCLUDE_SUPPLY_H_
#define _SUNXI_INCLUDE_SUPPLY_H_

#include <power/supply.h>
#include "sunxi_axp_pmic.h"

struct dm_supply_ops {
	/* Basic supply operations */
	int (*get_battery_exist_with_bat_vol)(struct udevice *dev);
	int (*get_battery_exist_with_bat_temp)(struct udevice *dev);
	int (*get_vbus_status)(struct udevice *dev);
	int (*get_battery_capacity)(struct udevice *dev);
	int (*get_battery_vol)(struct udevice *dev);
	int (*get_battery_status)(struct udevice *dev);
	int (*get_battery_temp)(struct udevice *dev, int param[]);
	int (*get_external_input_status)(struct udevice *dev);
	int (*get_external_input_limit)(struct udevice *dev);

	/* Control operations */
	int (*set_charge)(struct udevice *dev, bool enable);
	int (*set_ntc_onoff)(struct udevice *dev, bool enable, int ntc_cur);
	int (*set_coulombmeter_onoff)(struct udevice *dev, bool enable);
	int (*set_external_input_limit)(struct udevice *dev, int mA);
	int (*reset_capacity)(struct udevice *dev);
};

struct axp_supply_plat {
	const char *name;
};

#endif /* _SUNXI_INCLUDE_SUPPLY_H_ */
