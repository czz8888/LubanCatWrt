# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
#
# Based on some other Makefile
# (C) Copyright 2000-2003
# Wolfgang Denk, DENX Software Engineering, wd@denx.de.

obj-y	+= board.o
obj-y	+= clock.o
obj-$(CONFIG_AW_PLIC) += plic.o
obj-$(CONFIG_AW_CLIC) += aw_clic.o
obj-y	+= cpu_info.o
obj-y	+= rtc.o
obj-n	+= usb_phy.o
obj-$(CONFIG_SUNXI_GPIO_V1)$(CONFIG_SUNXI_GPIO_V2) += pinmux.o
obj-$(CONFIG_AXP_PMIC_BUS)	+= pmic_bus.o

extra-y      += spare_head.o

obj-$(CONFIG_MACH_SUN252IW1) += clock_sun252iw1.o board_sun252iw1.o
