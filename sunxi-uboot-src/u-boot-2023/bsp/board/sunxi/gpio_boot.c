/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/

#include <common.h>
#include <command.h>
#include <fdtdec.h>
#include <dm/ofnode.h>
#include <asm/gpio.h>
#include <dt-bindings/gpio/gpio.h>
#include <sunxi_board.h>
#include <linux/delay.h>
#include <adc.h>
#include <dm.h>
#include <power/pmic.h>
#include <asm/arch/rtc.h>

#define AXP2202_NAME      "pmu@35"

struct gpio_boot_priv {
	struct gpio_desc key_gpio;
	struct gpio_desc power_gpio;
	u32 check_time;
};

#ifndef CONFIG_BOOTCMD_SKIP_RTC
static int sunxi_get_bootcmd_from_rtc(void)
{
	u8 bootmode_flag = 0;

	bootmode_flag = rtc_get_bootmode_flag();
	debug("bootmode_flag = 0x%x", bootmode_flag);
	/*clear rtc*/
	rtc_set_bootmode_flag(0);
	switch (bootmode_flag) {
	case SUNXI_EFEX_CMD_FLAG:
		return SUNXI_EFEX_CMD_FLAG;
	case SUNXI_BOOT_RECOVERY_FLAG:
		return SUNXI_BOOT_RECOVERY_FLAG;
	case SUNXI_FASTBOOT_FLAG:
		return SUNXI_FASTBOOT_FLAG;
	case SUNXI_UBOOT_FLAG:
		return SUNXI_UBOOT_FLAG;
	case SUNXI_USER_POWEROFF_FLAG:
		return SUNXI_USER_POWEROFF_FLAG;
	default:
		return 0;
		break;
	}
}
#endif

static int gpio_boot_of_to_plat(struct udevice *dev)
{
	struct gpio_boot_priv *priv = dev_get_priv(dev);
	int ret;

	if (get_boot_work_mode() != WORK_MODE_BOOT)
			return 0;
	ret = gpio_request_by_name(dev, "key-gpios", 0, &priv->key_gpio, GPIOD_IS_IN);
	if (ret) {
			pr_notice("Failed to get key GPIO: %d\n", ret);
			return ret;
	}
	ret = gpio_request_by_name(dev, "power-gpios", 0, &priv->power_gpio, GPIOD_IS_OUT);
	if (ret) {
			pr_notice("Failed to get power GPIO: %d\n", ret);
			return ret;
	}
	priv->check_time = ofnode_read_u32_default(dev_ofnode(dev), "check-time-ms", 3000);

	return 0;
}
int gpio_boot_probe(struct udevice *dev)
{
	struct gpio_boot_priv *priv = dev_get_priv(dev);
	int gpio_value = 0;
	int ret;
	ulong begin_time = 0;
	static struct udevice *axp2202_dev;

	if (get_boot_work_mode() != WORK_MODE_BOOT) {
		return 0;
	}

	if (!dm_gpio_is_valid(&priv->key_gpio)) {
		printf("Invalid key GPIO\n");
		return -EINVAL;
	}
	if (!dm_gpio_is_valid(&priv->power_gpio)) {
		printf("Invalid power GPIO\n");
		return -EINVAL;
	}

	int rtc_val = sunxi_get_bootcmd_from_rtc();
	ret = pmic_get(AXP2202_NAME, &axp2202_dev);
	if (ret) {
		pr_notice("Can't get PMIC: %s!\n", AXP2202_NAME);
		return ret;
	}
	int	pmu_val = pmic_reg_read(axp2202_dev, 0x27);
	printf("[adbg][%s][%d]: pmu_val 0x%x, rtc_val 0x%x\n", __func__, __LINE__, pmu_val, rtc_val);

	if (rtc_val == SUNXI_USER_POWEROFF_FLAG || pmu_val == 0x4) {
		begin_time = get_timer(0);
		do {
			gpio_value = dm_gpio_get_value(&priv->key_gpio);
			if (gpio_value) {
				printf("[key recovery] no key press, power down\n");
				dm_gpio_set_value(&priv->power_gpio, 0);
			}
		} while (get_timer(begin_time) < priv->check_time);
	}

	return 0;
}
static const struct udevice_id gpio_boot_ids[] = {
	{ .compatible = "uboot,gpio-boot" },
	{ }
};
U_BOOT_DRIVER(gpio_boot) = {
	.name = "gpio_boot",
	.id = UCLASS_MISC,
	.of_match = gpio_boot_ids,
	.of_to_plat = gpio_boot_of_to_plat,
	.probe = gpio_boot_probe,
	.priv_auto = sizeof(struct gpio_boot_priv),
};
