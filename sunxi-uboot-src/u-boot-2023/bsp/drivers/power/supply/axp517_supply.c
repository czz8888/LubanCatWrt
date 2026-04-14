// z:\workspace\workspace\tina-v861\brandy\brandy-2.0\u-boot-bsp\drivers\power\supply\axp_supply.c
// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2024 AXP Supply Driver
 * Optimized supply framework consistent with regulator design
 */

#include <common.h>
#include <dm.h>
#include <errno.h>
#include <log.h>
#include "sunxi_supply.h"
#include "sunxi_axp_pmic.h"
#include <power/power_manage.h>
#include <linux/delay.h>

/* Helper functions for register access */
static inline int axp517_vbat_to_mV(u32 reg)
{
	return (int)(reg & 0x3FFF);
}

static inline int axp517_vts_to_mV(u32 reg)
{
	return (int)(reg & 0x3FFF) / 2;
}

static inline int axp517_bat_temp_mv(struct udevice *dev)
{
	int ret;
	int reg_value[2];
	u32 bat_temp_adc;
	int bat_temp_mv;
	struct udevice *parent = dev->parent;

	ret = pmic_reg_read(parent, AXP517_TS_H);
	if (ret < 0)
		return ret;
	reg_value[0] = ret & 0xFF;

	ret = pmic_reg_read(parent, AXP517_TS_L);
	if (ret < 0)
		return ret;
	reg_value[1] = ret & 0xFF;

	reg_value[0] &= 0x3F;
	bat_temp_adc = (reg_value[0] << 8) | reg_value[1];
	bat_temp_mv = axp517_vts_to_mV(bat_temp_adc);

	return bat_temp_mv;
}


static inline int axp_vts_to_temp(int data, int param[16])
{
	int temp;

	if (data < param[15])
		return 800;
	else if (data <= param[14]) {
		temp = 700 + (param[14]-data) * 100 /
		(param[14]-param[15]);
	} else if (data <= param[13]) {
		temp = 600 + (param[13]-data) * 100 /
		(param[13]-param[14]);
	} else if (data <= param[12]) {
		temp = 550 + (param[12]-data) * 50 /
		(param[12]-param[13]);
	} else if (data <= param[11]) {
		temp = 500 + (param[11]-data) * 50 /
		(param[11]-param[12]);
	} else if (data <= param[10]) {
		temp = 450 + (param[10]-data) * 50 /
		(param[10]-param[11]);
	} else if (data <= param[9]) {
		temp = 400 + (param[9]-data) * 50 /
		(param[9]-param[10]);
	} else if (data <= param[8]) {
		temp = 300 + (param[8]-data) * 100 /
		(param[8]-param[9]);
	} else if (data <= param[7]) {
		temp = 200 + (param[7]-data) * 100 /
		(param[7]-param[8]);
	} else if (data <= param[6]) {
		temp = 100 + (param[6]-data) * 100 /
		(param[6]-param[7]);
	} else if (data <= param[5]) {
		temp = 50 + (param[5]-data) * 50 /
		(param[5]-param[6]);
	} else if (data <= param[4]) {
		temp = 0 + (param[4]-data) * 50 /
		(param[4]-param[5]);
	} else if (data <= param[3]) {
		temp = -50 + (param[3]-data) * 50 /
		(param[3] - param[4]);
	} else if (data <= param[2]) {
		temp = -100 + (param[2]-data) * 50 /
		(param[2] - param[3]);
	} else if (data <= param[1]) {
		temp = -150 + (param[1]-data) * 50 /
		(param[1] - param[2]);
	} else if (data <= param[0]) {
		temp = -250 + (param[0]-data) * 100 /
		(param[0] - param[1]);
	} else
		temp = -250;
	return temp;
}

int axp517_set_ntc_cur(struct udevice *dev, int ntc_cur)
{
	struct udevice *parent = dev->parent;
	int ret;

	ret = pmic_reg_read(parent, AXP517_TS_CFG);
	ret &= 0xfc;

	if (ntc_cur < 40)
		ret |= 0x00;
	else if (ntc_cur < 50)
		ret |= 0x01;
	else if (ntc_cur < 60)
		ret |= 0x02;
	else
		ret |= 0x03;

	ret = pmic_reg_write(parent, AXP517_TS_CFG, ret);

	return ret;
}

static int axp517_set_ntc_onoff(struct udevice *dev, bool enable, int ntc_cur)
{
	struct udevice *parent = dev->parent;

	if (enable) {
		pmic_clrsetbits(parent, AXP517_TS_CFG, 0x10, 0x00); /* Clear disable bit */
		pmic_clrsetbits(parent, AXP517_ADC_CH_EN0, 0x02, 0x02); /* Enable TS ADC */
	} else {
		pmic_clrsetbits(parent, AXP517_TS_CFG, 0x10, 0x10); /* Set disable bit */
		pmic_clrsetbits(parent, AXP517_ADC_CH_EN0, 0x02,  0x00); /* Disable TS ADC */
		axp517_set_ntc_cur(dev, ntc_cur);
	}
	return 0;
}

static int axp517_set_charge(struct udevice *dev, bool enable)
{
	struct udevice *parent = dev->parent;

	if (!enable) {
		pmic_clrsetbits(parent, AXP517_MODULE_EN, BIT(1), 0x00); /* Clear disable bit */
	} else {
		pmic_clrsetbits(parent, AXP517_MODULE_EN, BIT(1), BIT(1)); /* Clear disable bit */
	}

	return 0;
}

static int axp517_get_battery_capacity(struct udevice *dev)
{
	int ret;
	struct udevice *parent = dev->parent;

	ret = pmic_reg_read(parent, AXP517_VBAT_H);
	if (ret < 0)
		return ret;
	ret = ret & 0x7F;

	return ret;
}

static int axp517_get_battery_vol(struct udevice *dev)
{
	int ret;
	int reg_value[2];
	u32 bat_vol_adc;
	struct udevice *parent = dev->parent;

	ret = pmic_reg_read(parent, AXP517_VBAT_H);
	if (ret < 0)
		return ret;
	reg_value[0] = ret & 0xFF;

	ret = pmic_reg_read(parent, AXP517_VBAT_L);
	if (ret < 0)
		return ret;
	reg_value[1] = ret & 0xFF;

	reg_value[0] &= 0x3F;
	bat_vol_adc = (reg_value[0] << 8) | reg_value[1];

	return axp517_vbat_to_mV(bat_vol_adc);
}

static int axp517_get_battery_temp(struct udevice *dev, int param[16])
{
	int bat_temp_mv, temp;

	bat_temp_mv = axp517_bat_temp_mv(dev);

	temp = axp_vts_to_temp(bat_temp_mv, (int *)param);

	return temp;
}

static int axp517_get_battery_exist_with_bat_vol(struct udevice *dev)
{
	int bat_vol;

	axp517_set_charge(dev, 0);

	__udelay(2 * 1000);
	bat_vol = axp517_get_battery_vol(dev);
	pr_notice("[AXP517] bat_vol when discharge:%d\n", bat_vol);
	axp517_set_charge(dev, 1);

	if (bat_vol < 3000)
		return 0;
	return 1;
}

static int axp517_get_battery_exist_with_bat_temp(struct udevice *dev)
{
	int temp_mv;

	pr_notice("%s:%d dev:%s\n", __func__, __LINE__, dev->name);
	/* get ts vol*/
	axp517_set_ntc_onoff(dev, 1, 50);
	temp_mv = axp517_bat_temp_mv(dev);
	pr_notice("[AXP517] check battery exist: battery temp_mv:%d\n", temp_mv);

	if (!temp_mv)
		return 0;

	return 1;
}

static int axp517_get_vbus_status(struct udevice *dev)
{
	int ret;
	struct udevice *parent = dev->parent;

	ret = pmic_reg_read(parent, AXP517_STATUS0);
	if (ret < 0)
		return ret;

	pr_notice("[AXP517] vbus status:%d\n", ret);

	ret &= BIT(5);

	return ret ? 1 : 0;
}

static int axp517_get_external_input_status(struct udevice *dev)
{
	int ret;
	struct udevice *parent = dev->parent;

	ret = pmic_reg_read(parent, AXP517_STATUS0);
	ret &= BIT(5);
	if (ret < 0)
		return ret;

	return ret ? EXTERNAL_INPUT_VBUS_EXIST : EXTERNAL_INPUT_NONE;
}

static int axp517_reset_capacity(struct udevice *dev)
{
	struct udevice *parent = dev->parent;

	return pmic_reg_write(parent, AXP517_GAUGE_CONFIG, 0x00);
}

static const struct dm_supply_ops axp517_supply_ops = {
	.get_battery_exist_with_bat_vol = axp517_get_battery_exist_with_bat_vol,
	.get_battery_exist_with_bat_temp = axp517_get_battery_exist_with_bat_temp,
	.get_vbus_status = axp517_get_vbus_status,
	.get_battery_capacity = axp517_get_battery_capacity,
	.get_battery_vol = axp517_get_battery_vol,
	.get_battery_temp = axp517_get_battery_temp,
	.get_external_input_status = axp517_get_external_input_status,
	.set_ntc_onoff = axp517_set_ntc_onoff,
	.reset_capacity = axp517_reset_capacity,
};

static int axp517_supply_probe(struct udevice *dev)
{
	int id = dev_get_driver_data(dev->parent);
	int reg_value;
	int bmu_chip_id;
	struct udevice *parent = dev->parent;

	bmu_chip_id = pmic_reg_read(parent, AXP517_CHIP_ID_EXT);
	if (bmu_chip_id != 0x4)
		return 0;

	pr_notice("%s:%d dev:%s\n", __func__, __LINE__, dev->name);
	pr_notice("%s:%d id:%d\n", __func__, __LINE__, id);

	/*
	 * when use this bit to shutdown system, it must write 0
	 * when system power on
	 */
	pmic_clrsetbits(parent, AXP517_BATFET_CTRL, BIT(3), 0x00);
	/* set input limit to 3A */
	pmic_reg_write(parent, AXP517_IIN_LIM, 0xE8);
	/* set cc clock enable */
	pmic_clrsetbits(parent, AXP517_CLK_EN, BIT(3), BIT(3));
	/* set vbus_ov to 11v */
	reg_value = pmic_reg_read(parent, AXP517_VBUS_OV_SET);
	reg_value &= ~(0xC0);
	reg_value |= 0x80;
	pmic_reg_write(parent, AXP517_VBUS_OV_SET, reg_value);
	/* set bc/cc changes disable */
	pmic_clrsetbits(parent, AXP517_BST_CFG1, BIT(7), BIT(7));
	pmic_clrsetbits(parent, 0xb, BIT(3), 0x00);
	/* disable bat det */
	pmic_clrsetbits(parent, AXP517_BAT_DET,  BIT(0), 0x00);
	/* disable frequency loop */
	pmic_reg_write(parent, AXP517_EXT_PARA0, 0x06);
	pmic_reg_write(parent, AXP517_EXT_PARA1, 0x04);
	pmic_reg_write(parent, AXP517_TWI_ADDR_EXT, 0x01);
	pmic_reg_write(parent, AXP517_RBFET_CTRL, 0x01);
	pmic_reg_write(parent, AXP517_TWI_ADDR_EXT, 0x00);
	/* set vimdpm to 4.7v */
	pmic_reg_write(parent, AXP517_VINDPM_CFG, 0x0b);
	/* set chg_freq to 1 MHz */
	pmic_reg_write(parent, AXP517_CHG_FREQ, 0x00);

	return 0;
}

U_BOOT_DRIVER(axp517_supply) = {
	.name = "axp517_supply",
	.id = UCLASS_SUNXI_POWER_SUPPLY,
	.probe = axp517_supply_probe,
	.ops = &axp517_supply_ops,
};