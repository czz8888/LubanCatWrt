// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2007-2012
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * (C) Copyright 2013 Luke Kenneth Casson Leighton <lkcl@lkcl.net>
 */

#include <common.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/gpio.h>
#include <asm/arch/sys_proto.h>

__weak void clock_init_sec(void)
{
}

__weak void gtbus_init(void)
{
}

int clock_init(void)
{
#ifdef CONFIG_SPL_BUILD
	clock_init_safe();
	gtbus_init();
#endif
	clock_init_uart();
	clock_init_sec();

	return 0;
}
