# SPDX-License-Identifier: GPL-2.0+
#
# Copyright (c) 2009 Wind River Systems, Inc.
# Tom Rix <Tom.Rix at windriver.com>

# include
ccflags-y += -I $(srctree)/bsp/drivers/power/include

# version
VERSION_FILE := $(srctree)/bsp/drivers/power/version
VERSION_PREFIX := SUNXI_POWER
VERSION := $(shell grep -m 1 'version:' $(VERSION_FILE) | awk '{print $$2}')
ccflags-y += -D$(VERSION_PREFIX)="\"$(VERSION)\""
$(info sunxi power version is $(VERSION))

# core
obj-$(CONFIG_AW_DM_PMIC)  		+= core/power_manage.o
obj-$(CONFIG_AW_PMIC_AXP)		+= core/axp.o

# regulator
obj-$(CONFIG_AW_REGULATOR_AXP)		+= regulator/axp_regulator.o

# supply
obj-$(CONFIG_AW_DM_SUPPLY)		+= supply/supply-uclass.o
obj-$(CONFIG_AW_SUPPLY_AXP517)		+= supply/axp517_supply.o

