// SPDX-License-Identifier: GPL-2.0 OR BSD-3-Clause
/*
 * Copyright (c) 2017-2019, ARM Limited and Contributors. All rights reserved.
 * Copyright (c) 2018-2023 Samuel Holland <samuel@sholland.org>
 */

#include <dm.h>
#include <errno.h>
#include <dm/device-internal.h>
#include <power/regulator.h>

#include "sunxi_axp_pmic.h"

#define NA 0xff

struct axp_regulator_plat {
	const char	*name;
	u8		enable_reg;
	u8		enable_mask;
	u8		volt_reg;
	u8		volt_mask;
	u16		min_mV;
	u16		max_mV;
	u8		step_mV;
	u8		split0;
	/* add dcdc mode sets */
	u8		dcdc_mode_reg;
	u8		dcdc_mode_mask;
	/* add for axp2101 */
	u8		split2;
	u8		factor2;
	u16		split_val2;
	/* end */
	const u16	*table;
};

static int axp_regulator_get_value(struct udevice *dev)
{
	const struct axp_regulator_plat *plat = dev_get_plat(dev);
	int mV, sel;

	if (plat->volt_reg == NA)
		return -EINVAL;

	sel = pmic_reg_read(dev->parent, plat->volt_reg);
	if (sel < 0)
		return sel;

	sel &= plat->volt_mask;
	sel >>= ffs(plat->volt_mask) - 1;

	if (plat->table) {
		mV = plat->table[sel];
	} else {
		mV = plat->min_mV + sel * plat->step_mV;
		if (sel > plat->split0) {
			if (plat->split2 && sel > (plat->split2 + plat->split0)) {
				sel = sel - plat->split2 - plat->split0 - 1;
				if (plat->split_val2) {
					mV = plat->split_val2 + sel * plat->factor2;
				} else {
					mV = plat->min_mV + (plat->split0 + plat->split2 * 2) * plat->step_mV + sel * plat->factor2;
				}
			} else {
				sel = plat->split0 + (sel - plat->split0) * 2;
				mV = plat->min_mV + sel * plat->step_mV;
			}
		}
	}

	return mV * 1000;
}

static int axp_regulator_set_value(struct udevice *dev, int uV)
{
	const struct axp_regulator_plat *plat = dev_get_plat(dev);
	int mV = uV / 1000;
	uint sel, shift;

	if (plat->volt_reg == NA)
		return -EINVAL;
	if (mV < plat->min_mV || mV > plat->max_mV)
		return -EINVAL;

	shift = ffs(plat->volt_mask) - 1;

	if (plat->table) {
		/*
		 * The table must be monotonically increasing and
		 * have an entry for each possible field value.
		 */
		sel = plat->volt_mask >> shift;
		while (sel && plat->table[sel] > mV)
			sel--;
	} else {
		sel = (mV - plat->min_mV) / plat->step_mV;
		if (plat->split2 && sel > (plat->split2 + plat->split0)) {
				if (plat->split_val2)
					sel = (mV - plat->split_val2 - plat->min_mV) / plat->factor2;
				else
					sel = (mV - (plat->split2 * 2 + plat->split0) * plat->step_mV - plat->min_mV) / plat->factor2;
				sel = sel + plat->split2 + plat->split0 + 1;
			} else if (sel > plat->split0) {
				sel = plat->split0 + (sel - plat->split0) / 2;
			}
	}

	return pmic_clrsetbits(dev->parent, plat->volt_reg,
			       plat->volt_mask, sel << shift);
}

static int axp_regulator_get_enable(struct udevice *dev)
{
	const struct axp_regulator_plat *plat = dev_get_plat(dev);
	int reg;

	reg = pmic_reg_read(dev->parent, plat->enable_reg);
	if (reg < 0)
		return reg;

	return (reg & plat->enable_mask) == plat->enable_mask;
}

static int axp_regulator_set_enable(struct udevice *dev, bool enable)
{
	const struct axp_regulator_plat *plat = dev_get_plat(dev);

	return pmic_clrsetbits(dev->parent, plat->enable_reg,
			       plat->enable_mask,
			       enable ? plat->enable_mask : 0);
}

static int axp_regulator_get_dcdc_mode(struct udevice *dev)
{
	const struct axp_regulator_plat *plat = dev_get_plat(dev);
	int reg;

	if (strstr((const char *)dev->name, "dcdc") == NULL) {
		return 0;
	}

	reg = pmic_reg_read(dev->parent, plat->dcdc_mode_reg);
	if (reg < 0)
		return reg;

	return (reg & plat->dcdc_mode_mask) == plat->dcdc_mode_mask;
}

static int axp_regulator_set_dcdc_mode(struct udevice *dev, int enable)
{
	const struct axp_regulator_plat *plat = dev_get_plat(dev);

	if (strstr((const char *)dev->name, "dcdc") == NULL) {
		return 0;
	}

	return pmic_clrsetbits(dev->parent, plat->dcdc_mode_reg,
			       plat->dcdc_mode_mask,
			       enable ? plat->dcdc_mode_mask : 0);
}

static const struct dm_regulator_ops axp_regulator_ops = {
	.get_value		= axp_regulator_get_value,
	.set_value		= axp_regulator_set_value,
	.get_enable		= axp_regulator_get_enable,
	.set_enable		= axp_regulator_set_enable,
	.get_mode		= axp_regulator_get_dcdc_mode,
	.set_mode		= axp_regulator_set_dcdc_mode,
};

static const struct axp_regulator_plat axp2101_regulators[] = {
	{ "dcdc1", 0x80, BIT(0), 0x82, 0x1f, 1500, 3400, 100, NA, 0x81, BIT(2)},
	{ "dcdc2", 0x80, BIT(1), 0x83, 0x7f,  500, 1540,  10, 70, 0x81, BIT(3)},
	{ "dcdc3", 0x80, BIT(2), 0x84, 0x7f,  500, 1540,  10, 70, 0x81, BIT(4), 17, 100, 1600},
	{ "dcdc4", 0x80, BIT(3), 0x85, 0x7f,  500, 1840,  10, 70, 0x81, BIT(5)},
	{ "dcdc5", 0x80, BIT(4), 0x86, 0x1f, 1400, 3700, 100, NA },
	{ "aldo1", 0x90, BIT(0), 0x92, 0x1f,  500, 3500, 100, NA },
	{ "aldo2", 0x90, BIT(1), 0x93, 0x1f,  500, 3500, 100, NA },
	{ "aldo3", 0x90, BIT(2), 0x94, 0x1f,  500, 3500, 100, NA },
	{ "aldo4", 0x90, BIT(3), 0x95, 0x1f,  500, 3500, 100, NA },
	{ "bldo1", 0x90, BIT(4), 0x96, 0x1f,  500, 3500, 100, NA },
	{ "bldo2", 0x90, BIT(5), 0x97, 0x1f,  500, 3500, 100, NA },
	{ "dldo1", 0x90, BIT(7), 0x99, 0x1f,  500, 3500, 100, NA },
	{ "dldo2", 0x91, BIT(0), 0x9a, 0x1f,  500, 3500,  50, NA },
	{ "cpusldo", 0x90, BIT(6), 0x98, 0x1f,  500, 1400, 50, NA },
	{ }
};

static const struct axp_regulator_plat axp2202_regulators[] = {
	{ "dcdc1", 0x80, BIT(0), 0x83, 0x7f,  500, 1540,  10, 70, 0x81, BIT(2)},
	{ "dcdc2", 0x80, BIT(1), 0x84, 0x7f,  500, 3400,  10, 70, 0x81, BIT(3), 17, 100, 1600},
	{ "dcdc3", 0x80, BIT(2), 0x85, 0x7f,  500, 1840,  10, 70, 0x81, BIT(4)},
	{ "dcdc4", 0x80, BIT(3), 0x86, 0x1f, 1000, 3700, 100, NA },
	{ "aldo1", 0x90, BIT(0), 0x93, 0x1f,  500, 3500, 100, NA },
	{ "aldo2", 0x90, BIT(1), 0x94, 0x1f,  500, 3500, 100, NA },
	{ "aldo3", 0x90, BIT(2), 0x95, 0x1f,  500, 3500, 100, NA },
	{ "aldo4", 0x90, BIT(3), 0x96, 0x1f,  500, 3500, 100, NA },
	{ "bldo1", 0x90, BIT(4), 0x97, 0x1f,  500, 3500, 100, NA },
	{ "bldo2", 0x90, BIT(5), 0x98, 0x1f,  500, 3500, 100, NA },
	{ "bldo3", 0x90, BIT(6), 0x99, 0x1f,  500, 3500, 100, NA },
	{ "bldo4", 0x90, BIT(7), 0x9a, 0x1f,  500, 3500, 100, NA },
	{ "cldo1", 0x91, BIT(0), 0x9b, 0x1f,  500, 3500, 100, NA },
	{ "cldo2", 0x91, BIT(1), 0x9c, 0x1f,  500, 3500, 100, NA },
	{ "cldo3", 0x91, BIT(2), 0x9d, 0x1f,  500, 3500, 100, NA },
	{ "cldo4", 0x91, BIT(3), 0x9e, 0x1f,  500, 3500, 100, NA },
	{ "cpusldo", 0x91, BIT(4), 0x9f, 0x1f,  500, 1400, 50, NA },
	{ }
};

static const struct axp_regulator_plat axp1530_regulators[] = {
	{ "dcdc1", 0x10, BIT(0), 0x13, 0x7f,  500, 3400,  10, 70, 0x12, BIT(0), 17, 100, 1600},
	{ "dcdc2", 0x10, BIT(1), 0x14, 0x7f,  500, 1540,  10, 70, 0x12, BIT(1)},
	{ "dcdc3", 0x10, BIT(2), 0x15, 0x7f,  500, 1840,  10, 70, 0x12, BIT(2)},
	{ "aldo1", 0x10, BIT(3), 0x16, 0x1f,  500, 3500, 100, NA },
	{ "dldo1", 0x10, BIT(4), 0x17, 0x1f,  500, 3500, 100, NA },
	{ }
};

static const struct axp_regulator_plat *const axp_regulators[] = {
	[AXP2101_ID]	= axp2101_regulators,
	[AXP2202_ID]	= axp2202_regulators,
	[AXP1530_ID]	= axp1530_regulators,
	[AXP333_ID]	= axp333_regulators,
};

static int axp_regulator_bind(struct udevice *dev)
{
	struct dm_regulator_uclass_plat *uc_plat = dev_get_uclass_plat(dev);
	ulong id = dev_get_driver_data(dev->parent);
	const struct axp_regulator_plat *plat;

	for (plat = axp_regulators[id]; plat && plat->name; plat++)
		if (!strcmp(plat->name, dev->name))
			break;
	if (!plat || !plat->name)
		return -ENODEV;

	dev_set_plat(dev, (void *)plat);

	if (plat->volt_reg == NA)
		uc_plat->type = REGULATOR_TYPE_FIXED;
	else if (!strncmp(plat->name, "dcdc", strlen("dcdc")))
		uc_plat->type = REGULATOR_TYPE_BUCK;
	else
		uc_plat->type = REGULATOR_TYPE_LDO;

	return 0;
}

U_BOOT_DRIVER(axp_regulator) = {
	.name		= "sunxi_axp_regulator",
	.id		= UCLASS_REGULATOR,
	.bind		= axp_regulator_bind,
	.ops		= &axp_regulator_ops,
};
