/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2015 Hans de Goede <hdegoede@redhat.com>
 *
 * X-Powers AX Power Management IC support header
 */
#ifndef _SUNXI_AXP_PMIC_H_
#define _SUNXI_AXP_PMIC_H_

#include <stdbool.h>

#include <power/power_manage.h>
#include "axp2202.h"
#include "axp2101.h"
#include "axp333.h"
#include "axp517.h"
#include "sunxi_supply.h"

#define FDT_PATH_CHARGER0		"/soc/charger0"
#define FDT_PATH_POWER_DELAY		"/soc/power_delay"
#define FDT_PATH_POWER_SPLY		"/soc/power_sply"
#define FDT_PATH_GPIO_BIAS		"/soc/gpio_bias"

enum {
	AXP2101_ID,
	AXP2202_ID,
	AXP1530_ID,
	AXP333_ID,
	AXP517_ID,
	AXP_ID_MAX,
};

static const char *const axp_model_names[] = {
	"axp2101", "axp2202", "axp1530", "axp333", "axp517"
};

int axp_start_up_debug(struct udevice *dev);
int axp_set_power_off(void);

#endif
