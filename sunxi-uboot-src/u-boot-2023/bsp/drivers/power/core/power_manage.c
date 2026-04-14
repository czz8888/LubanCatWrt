/*
 * Copyright (C) 2019 Allwinner.
 * weidonghui <weidonghui@allwinnertech.com>
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#include <spare_head.h>
#include <console.h>
#include <dm.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <i2c.h>
#include <dm/uclass.h>
#include <linux/delay.h>
#include <sunxi_board.h>

#include "sunxi_axp_pmic.h"

/*
 * Global data (for the gd->bd)
 */
DECLARE_GLOBAL_DATA_PTR;

int pmic_set_power_supply_output(void)
{
	int nodeoffset = -1;
	int ret, i;
	struct udevice *dev;
	char power_name[32];
	int power_vol, power_vol_d, onoff;
	ulong id;


	nodeoffset = fdt_path_offset(working_fdt, FDT_PATH_POWER_SPLY);
	if (nodeoffset < 0) {
		pr_err("Power supply node not found at path %s\n", FDT_PATH_POWER_SPLY);
		return nodeoffset;
	}

	for (i = 0; ; i++) {
		ret = uclass_get_device_by_seq(UCLASS_REGULATOR, i, &dev);
		if (ret == -ENODEV)
			break;

		id = dev_get_driver_data(dev->parent);

		memset(power_name, 0, sizeof(power_name));
		strcpy(power_name, axp_model_names[id]);
		strcat(power_name, "_");
		strcat(power_name, dev->name);
		strcat(power_name, "_vol");

		power_vol = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, power_name, -1);
		if (power_vol < 0) {
			pr_notice("PMIC: No voltage config for %s, skipping\n", power_name);
			continue;
		}

		onoff       = -1;
		power_vol_d = 0;

		if (power_vol > 10000) {
			onoff       = 1;
			power_vol_d = power_vol % 10000;

		} else if (power_vol >= 0) {
			onoff       = 0;
			power_vol_d = power_vol;
		}

		regulator_set_enable(dev, onoff);
		regulator_set_value(dev, power_vol_d * 1000);

		printf("%s = %d[now:%d], onoff=%d[now:%d]\n",
			   power_name, power_vol_d,
			   regulator_get_value(dev) / 1000,
			   onoff, regulator_get_enable(dev));
	}

	pr_notice("PMIC: Power supply output configuration completed\n");
	return 0;
}

/* set dcdc pwm mode */
int pmic_set_dcdc_mode(void)
{
	int nodeoffset = -1;
	int ret, i;
	struct udevice *dev;
	char power_name[32];
	int dcdc_mode;
	ulong id;

	nodeoffset = fdt_path_offset(working_fdt, FDT_PATH_POWER_SPLY);
	if (nodeoffset < 0) {
		pr_err("PMIC: Power supply node not found at path %s\n", FDT_PATH_POWER_SPLY);
		return nodeoffset;
	}

	for (i = 0; ; i++) {
		ret = uclass_get_device_by_seq(UCLASS_REGULATOR, i, &dev);
		if (ret == -ENODEV)
			break;

		if (strstr(dev->name, "dcdc") == NULL) {
			pr_notice("PMIC: Skipping non-DCDC device %s\n", dev->name);
			continue;
		}

		id = dev_get_driver_data(dev->parent);
		memset(power_name, 0, sizeof(power_name));
		strcpy(power_name, axp_model_names[id]);
		strcat(power_name, "_");
		strcat(power_name, dev->name);
		strcat(power_name, "_mode");

		dcdc_mode = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, power_name, -1);
		if (dcdc_mode < 0) {
			pr_notice("PMIC: No mode config for %s, skipping\n", power_name);
			continue;
		}

		if (regulator_set_mode(dev, dcdc_mode) < 0)
			pr_err("PMIC: Failed to set %s to mode %d\n", power_name, dcdc_mode);
		else
			pr_notice("PMIC: Successfully set %s = %d[now:%d]\n", power_name, dcdc_mode, regulator_get_mode(dev));
	}

	pr_notice("PMIC: DCDC mode configuration completed\n");
	return 0;
}

#if CONFIG_IS_ENABLED(CONFIG_AW_DM_SUPPLY)
int pmic_check_power_on(struct udevice *dev)
{
	int radio, bat_vol, bat_temp;
	int nodeoffset = -1, i, j;
	static int init, node_val[5];
	static int para[16];
	char para_name[32];

	nodeoffset = fdt_path_offset(working_fdt, FDT_PATH_CHARGER0);
	if (nodeoffset < 0)
		return nodeoffset;

	if (!init) {
		node_val[0] = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, "ntc_status", 0);
		node_val[1] = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, "pmu_safe_vol", 3400);
		node_val[2] = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, "pmu_safe_ratio", 0);
		node_val[3] = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, "safe_temp_H", 600);
		node_val[4] = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, "safe_temp_L", -50);

		for (i = 0; i < 16; i++) {
			j = i + 1;
			sprintf(para_name, "pmu_bat_temp_para%d", j);
			para[i] = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, para_name, 498);
		}

		init = 1;
	}

	radio = pmic_get_battery_capacity(dev);
	bat_vol = pmic_get_battery_vol(dev);
	bat_temp = pmic_get_battery_temp(dev, para);

	printf("battery capacity:%d, safe_ratio:%d\n", radio, node_val[2]);
	printf("battery vol:%d, safe_vol:%d\n", bat_vol, node_val[1]);
	printf("battery temp:%d, safe_temp_H:%d, safe_temp_L:%d\n", bat_temp, node_val[3], node_val[4]);

	if (radio < node_val[2])
		return 0;

	if (bat_vol < node_val[1])
		return 0;

	if (node_val[0]) {
		if (bat_temp > node_val[3])
			return 0;

		if (bat_temp < node_val[4])
			return 0;
	}

	return 1;
}

int pmic_get_battery_exist(struct udevice *dev)
{
	int nodeoffset = -1;
	int ts_bat_detect, bat_exist;
	int vbus_exist;

	nodeoffset = fdt_path_offset(working_fdt, FDT_PATH_POWER_SPLY);
	if (nodeoffset < 0)
		return 0;

	bat_exist = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, "battery_exist", 1);
	if (!bat_exist)
		return 0;

	vbus_exist = pmic_get_vbus_status(dev);

	if (!vbus_exist)
		return 1;

	ts_bat_detect = fdt_getprop_u32_default_node(working_fdt, nodeoffset, 0, "ts_bat_detect", 1);

	if (ts_bat_detect)
		bat_exist = pmic_get_battery_exist_with_bat_temp(dev);
	else
		bat_exist = pmic_get_battery_exist_with_bat_vol(dev);

	return bat_exist;
}

int pmic_get_battery_status(struct udevice *dev)
{
	int bat_exist;

	bat_exist = pmic_get_battery_exist(dev);
	printf("battery bat_exist:%d\n", bat_exist);

	if (bat_exist) {
		while (!pmic_check_power_on(dev)) {
				sunxi_board_shutdown();
			 __udelay(10 * 1000);
			if (ctrlc())
				break;
		}
	}

	return 0;
}
#endif

int pmic_probe(void)
{
	int i, err, ret;
	struct uclass *uc;
	struct udevice *dev;

	err = uclass_get(UCLASS_PMIC, &uc);
	if (err)
		return err;

	for (i = 0; ; i++) {
		err = uclass_get_device_by_seq(UCLASS_PMIC, i, &dev);
		if (err == -ENODEV)
			break;
		ret = err;
		pr_notice("%s:%d dev:%s\n", __func__, __LINE__, dev->name);
	}

	uclass_foreach_dev(dev, uc) {
		err = device_probe(dev);
		if (err)
			pr_err("%s - probe failed: %d\n", dev->name, err);
		else
			axp_start_up_debug(dev);
	}

	for (i = 0; ; i++) {
		err = uclass_get_device_by_seq(UCLASS_SUNXI_POWER_SUPPLY, i, &dev);
		if (err == -ENODEV)
			break;
		pmic_get_battery_status(dev);
		pr_notice("%s:%d dev:%s\n", __func__, __LINE__, dev->name);
	}

	printf("%s:%d, ret:%d\n", __func__, __LINE__, ret);
	return ret;
}
