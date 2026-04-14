/*
 * Copyright (C) 2019 Allwinner.
 * weidonghui <weidonghui@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef _INCLUDE_SUPPLY_H_
#define _INCLUDE_SUPPLY_H_

enum battery_exist {
	BATTERY_FAULT = -EINVAL,
	BATTERY_NONE = 0,
	BATTERY_IS_EXIST,
	BATTERY_EXIST_STATUS_MAX,
};

enum battery_status {
	BATTERY_STATUS_FAULT = -EINVAL,
	BATTERY_STATUS_NONE = 0,
	BATTERY_STATUS_NORMAL,
	BATTERY_STATUS_VOLTAGE_LOW,
	BATTERY_STATUS_RATIO_LOW,
	BATTERY_STATUS_OVERTEMP,
	BATTERY_STATUS_UNDERTEMP,
	BATTERY_STATUS_MAX,
};

enum external_input_status {
	EXTERNAL_INPUT_FAULT = -EINVAL,
	EXTERNAL_INPUT_UNKNOWN = 0,
	EXTERNAL_INPUT_NONE,
	EXTERNAL_INPUT_VBUS_EXIST,
	EXTERNAL_INPUT_DCIN_EXIST,
	EXTERNAL_INPUT_STATUS_MAX
};

struct dm_supply_uclass_plat {
	const char *name;
	struct dm_supply_ops *ops;
};

int pmic_get_battery_exist_with_bat_vol(struct udevice *dev);
int pmic_get_battery_exist_with_bat_temp(struct udevice *dev);
int pmic_get_vbus_status(struct udevice *dev);
int pmic_get_battery_capacity(struct udevice *dev);
int pmic_get_battery_vol(struct udevice *dev);
int pmic_get_battery_temp(struct udevice *dev, int param[]);
int pmic_get_external_input_status(struct udevice *dev);
int pmic_get_external_input_limit(struct udevice *dev);
int pmic_set_charge_enable(struct udevice *dev, bool enable);
int pmic_set_ntc_onoff(struct udevice *dev, bool enable, int ntc_cur);
int pmic_set_coulombmeter_onoff(struct udevice *dev, bool enable);
int pmic_set_external_input_limit(struct udevice *dev, int mA);
int pmic_get_external_input_limit(struct udevice *dev);

#endif /* _INCLUDE_SUPPLY_H_ */



