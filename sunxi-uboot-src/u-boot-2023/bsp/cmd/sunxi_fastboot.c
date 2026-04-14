/*
 * Copyright 2008 - 2009 Windriver, <www.windriver.com>
 * Author: Tom Rix <Tom.Rix@windriver.com>
 *
 * (C) Copyright 2014 Linaro, Ltd.
 * Rob Herring <robh@kernel.org>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <common.h>
#include <command.h>
//#include <g_dnl.h>

#define SUNXI_USB_DEVICE_FASTBOOT 3
int sunxi_usb_main_loop(uint dev_name, int delaytime);

static int do_fastboot(struct cmd_tbl *cmdtp, int flag, int argc, char *const argv[])
{

	if (sunxi_usb_main_loop(SUNXI_USB_DEVICE_FASTBOOT, 0)) {
		printf("usb fastboot fail: not support sunxi fastboot\n");
		return -1;
	}

	return 0;
}

U_BOOT_CMD(fastboot, 1, 1, do_fastboot,
	   "fastboot - enter USB Fastboot protocol", "");
