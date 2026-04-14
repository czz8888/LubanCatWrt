// SPDX-License-Identifier: GPL-2.0+

#include <dm.h>
#include <dm/lists.h>
#include <i2c.h>
#include <sysreset.h>
#include <sunxi_board.h>
#include <console.h>
#include <linux/delay.h>

#include "sunxi_axp_pmic.h"

int _axp_set_power_off(struct udevice *dev)
{
	int ret, id;

	id = dev_get_driver_data(dev);
	switch (id) {
	case AXP2202_ID:
		ret = pmic_clrsetbits(dev, AXP2202_OFF_CTL, AXP2202_POWEROFF, AXP2202_POWEROFF);
		if (ret < 0)
			return ret;
		while (1)
		;
		break;
	case AXP2101_ID:
		ret = pmic_clrsetbits(dev, AXP2101_OFF_CTL, AXP2101_POWEROFF, AXP2101_POWEROFF);
		if (ret < 0)
			return ret;
		while (1)
		;
		break;
	case AXP333_ID:
		ret = pmic_clrsetbits(dev, AXP333_OFF_CTL, AXP333_POWEROFF, AXP333_POWEROFF);
		if (ret < 0)
			return ret;
		while (1)
		;
		break;
	default:
		pr_notice("no axp poweroff function");
		return -EPROTONOSUPPORT;
	}

	return 0;
}

int axp_set_power_off(void)
{
	int ret, i;
	struct uclass *uc;
	struct udevice *dev;
	bool no_power_off;

	ret = uclass_get(UCLASS_PMIC, &uc);
	if (ret)
		return ret;

	for (i = 0; ; i++) {
		ret = uclass_get_device_by_seq(UCLASS_PMIC, i, &dev);
		if (ret == -ENODEV)
			break;
		no_power_off = dev_read_bool(dev, "axp-poweroff-disable");
		if (no_power_off)
			continue;
		ret = _axp_set_power_off(dev);
	}
	return ret;
}

#if CONFIG_IS_ENABLED(SYSRESET)
static int axp_sysreset_request(struct udevice *dev, enum sysreset_t type)
{
	if (type != SYSRESET_POWER_OFF)
		return -EPROTONOSUPPORT;

	return axp_set_power_off();
}

static struct sysreset_ops axp_sysreset_ops = {
	.request	= axp_sysreset_request,
};

U_BOOT_DRIVER(axp_sysreset) = {
	.name		= "axp_sysreset",
	.id		= UCLASS_SYSRESET,
	.ops		= &axp_sysreset_ops,
};
#endif

static int axp_pmic_reg_count(struct udevice *dev)
{
	/* TODO: Get the specific value from driver data. */
	return 0x100;
}

static struct dm_pmic_ops axp_pmic_ops = {
	.reg_count	= axp_pmic_reg_count,
	.read		= dm_i2c_read,
	.write		= dm_i2c_write,
};

int axp2202_reg_debug(struct udevice *dev)
{
	pr_notice("[AXP2202] comm status : 0x%x = 0x%x, 0x%x = 0x%x\n",
				AXP2202_COMM_STATUS0, pmic_reg_read(dev, AXP2202_COMM_STATUS0),
				AXP2202_MODE_CHGSTATUS, pmic_reg_read(dev, AXP2202_MODE_CHGSTATUS));
	pr_notice("[AXP2202] onoff status : 0x%x = 0x%x, 0x%x = 0x%x\n",
				AXP2202_PWRON_STATUS, pmic_reg_read(dev, AXP2202_PWRON_STATUS),
				AXP2202_PWROFF_STATUS, pmic_reg_read(dev, AXP2202_PWROFF_STATUS));
	pr_notice("[[AXP2202] reboot/charge status: 0x%x = 0x%x\n",
				AXP2202_DATA_BUFFER3, pmic_reg_read(dev, AXP2202_DATA_BUFFER3));
	pr_notice("[[AXP2202] reboot/charge status: 0x%x = 0x%x\n",
				AXP2202_IIN_LIM, pmic_reg_read(dev, AXP2202_IIN_LIM));

	return 0;
}

int axp333_reg_debug(struct udevice *dev)
{
	pr_notice("[AXP333] onoff status : 0x%x = 0x%x, 0x%x = 0x%x\n",
				AXP333_PWRON_STATUS, pmic_reg_read(dev, AXP333_PWRON_STATUS),
				AXP333_PWROFF_STATUS, pmic_reg_read(dev, AXP333_PWROFF_STATUS));

	return 0;
}


int axp517_reg_debug(struct udevice *dev)
{
	pr_notice("[AXP517] onoff status : 0x%x = 0x%x\n",
				AXP517_IRQ1_STATUS, pmic_reg_read(dev, AXP517_IRQ1_STATUS));

	return 0;
}

int axp_start_up_debug(struct udevice *dev)
{
	int id;

	id = dev_get_driver_data(dev);
	switch (id) {
	case AXP2202_ID:
		axp2202_reg_debug(dev);
		break;
	case AXP333_ID:
		axp333_reg_debug(dev);
		break;
	case AXP517_ID:
		axp517_reg_debug(dev);
		break;
	default:
		pr_notice("no axp start up debug function\n");
	}

	return 0;
}

static const struct pmic_child_info axp_pmu_child_info[] = {
	{ "aldo",	"sunxi_axp_regulator" },
	{ "bldo",	"sunxi_axp_regulator" },
	{ "cldo",	"sunxi_axp_regulator" },
	{ "dc",		"sunxi_axp_regulator" },
	{ "dldo",	"sunxi_axp_regulator" },
	{ "eldo",	"sunxi_axp_regulator" },
	{ "fldo",	"sunxi_axp_regulator" },
	{ "ldo",	"sunxi_axp_regulator" },
	{ "sw",		"sunxi_axp_regulator" },
	{ "cpusldo",	"sunxi_axp_regulator" },
	{ }
};

static const struct pmic_child_info axp_bmu_child_info[] = {
	{ "supply",	"axp517_supply" },
	{ }
};

static int axp_pmic_bind(struct udevice *dev)
{
	ofnode node;
	int ret;

	ret = dm_scan_fdt_dev(dev);
	if (ret)
		return ret;

	node = dev_read_subnode(dev, "regulators");
	if (ofnode_valid(node))
		pmic_bind_children(dev, node, axp_pmu_child_info);

	node = dev_read_subnode(dev, "supplys");
	if (ofnode_valid(node))
		pmic_bind_children(dev, node, axp_bmu_child_info);

	if (CONFIG_IS_ENABLED(SYSRESET)) {
		ret = device_bind_driver_to_node(dev, "axp_sysreset", "axp_sysreset",
						 dev_ofnode(dev), NULL);
		if (ret)
			return ret;
	}

	return 0;
}

static const struct udevice_id axp_pmic_ids[] = {
	{ .compatible = "x-powers,axp2101", .data = AXP2101_ID },
	{ .compatible = "x-powers,axp2202", .data = AXP2202_ID },
	{ .compatible = "x-powers,axp1530", .data = AXP1530_ID },
	{ .compatible = "x-powers,axp333", .data = AXP333_ID },
	{ .compatible = "x-powers,axp517", .data = AXP517_ID },
	{ }
};

U_BOOT_DRIVER(axp_pmic) = {
	.name		= "sunxi_axp_pmic",
	.id		= UCLASS_PMIC,
	.of_match	= axp_pmic_ids,
	.bind		= axp_pmic_bind,
	.ops		= &axp_pmic_ops,
};
