// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 AXP Supply Management
 * Optimized supply framework with efficient device management
 */

#define LOG_CATEGORY UCLASS_SUNXI_POWER_SUPPLY

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include <malloc.h>
#include <dm/uclass-internal.h>
#include <power/supply.h>
#include "sunxi_supply.h"

int pmic_get_battery_exist_with_bat_vol(struct udevice *dev)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->get_battery_exist_with_bat_vol)
		return -ENOSYS;

	return ops->get_battery_exist_with_bat_vol(dev);
}

int pmic_get_battery_exist_with_bat_temp(struct udevice *dev)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->get_battery_exist_with_bat_temp)
		return -ENOSYS;

	return ops->get_battery_exist_with_bat_temp(dev);
}

int pmic_get_vbus_status(struct udevice *dev)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->get_vbus_status)
		return -ENOSYS;

	return ops->get_vbus_status(dev);
}

int pmic_get_battery_capacity(struct udevice *dev)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->get_battery_capacity)
		return -ENOSYS;

	return ops->get_battery_capacity(dev);
}

int pmic_get_battery_vol(struct udevice *dev)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->get_battery_vol)
		return -ENOSYS;

	return ops->get_battery_vol(dev);
}

int pmic_get_battery_temp(struct udevice *dev, int param[])
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->get_battery_temp)
		return -ENOSYS;

	return ops->get_battery_temp(dev, param);
}


int pmic_get_external_input_status(struct udevice *dev)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->get_external_input_status)
		return -ENOSYS;

	return ops->get_external_input_status(dev);
}

int pmic_get_external_input_limit(struct udevice *dev)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->get_external_input_limit)
		return -ENOSYS;

	return ops->get_external_input_limit(dev);
}

int pmic_set_charge_enable(struct udevice *dev, bool enable)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->set_charge)
		return -ENOSYS;

	return ops->set_charge(dev, enable);
}

int pmic_set_ntc_onoff(struct udevice *dev, bool enable, int ntc_cur)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->set_ntc_onoff)
		return -ENOSYS;

	return ops->set_ntc_onoff(dev, enable, ntc_cur);
}

int pmic_set_coulombmeter_onoff(struct udevice *dev, bool enable)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->set_coulombmeter_onoff)
		return -ENOSYS;

	return ops->set_coulombmeter_onoff(dev, enable);
}

int pmic_set_external_input_limit(struct udevice *dev, int mA)
{
	const struct dm_supply_ops *ops = dev_get_driver_ops(dev);

	if (!ops || !ops->set_external_input_limit)
		return -ENOSYS;

	return ops->set_external_input_limit(dev, mA);
}

/* Supply uclass driver */
UCLASS_DRIVER(supply) = {
	.id		= UCLASS_SUNXI_POWER_SUPPLY,
	.name		= "sunxi_supply",
	.per_device_plat_auto	= sizeof(struct dm_supply_uclass_plat),
};