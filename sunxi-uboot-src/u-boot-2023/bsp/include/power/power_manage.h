/*
 * Copyright (C) 2019 Allwinner.
 * weidonghui <weidonghui@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */
#ifndef _POWER_MANAGE_H_
#define _POWER_MANAGE_H_

#include <common.h>
#include <command.h>
#include <linux/ctype.h>
#include <linux/types.h>
#include <asm/global_data.h>
#include <linux/libfdt.h>
#include <fdt_support.h>

#include <power/pmic.h>
#include <power/regulator.h>
#include <power/supply.h>

#if CONFIG_IS_ENABLED(AW_DM_PMIC)

int pmic_probe(void);

int pmic_set_power_supply_output(void);

int pmic_set_dcdc_mode(void);

#else

static inline int pmic_probe(void)
{
	return -ENOSYS;
}

static inline int pmic_set_power_supply_output(void)
{
	return -ENOSYS;
}

static inline int pmic_set_dcdc_mode(void)
{
	return -ENOSYS;
}

#endif

#if CONFIG_IS_ENABLED(CONFIG_AW_DM_SUPPLY)

int pmic_check_power_on(struct udevice *dev);

int pmic_get_battery_status(struct udevice *dev);

int pmic_get_battery_exist(struct udevice *dev);

#else

static inline int pmic_check_power_on(struct udevice *dev)
{
	return -ENOSYS;
}

static inline int pmic_get_battery_status(struct udevice *dev)
{
	return -ENOSYS;
}

static inline int pmic_get_battery_exist(struct udevice *dev)
{
	return -ENOSYS;
}

#endif

#endif


