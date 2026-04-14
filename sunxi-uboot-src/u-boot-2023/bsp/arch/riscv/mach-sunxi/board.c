// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2012 Henrik Nordstrom <henrik@henriknordstrom.net>
 *
 * (C) Copyright 2007-2011
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <tangliang@allwinnertech.com>
 *
 * Some init for sunxi platform.
 */

#include <common.h>
#include <mmc.h>
#include <i2c.h>
#include <serial.h>
#include <spl.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/spl.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/timer.h>
#include <asm/arch/mmc.h>
#include <cpu_func.h>

#include <linux/compiler.h>

#include <spare_head.h>
#include <private_uboot.h>
#include <asm/arch/efuse.h>
#include <asm/sections.h>

static int gpio_init(void)
{
	return 0;
}

void s_init(void)
{

#if !defined(CONFIG_ARM_CORTEX_CPU_IS_UP) && !defined(CONFIG_ARM64) \
	&& defined(CONFIG_ARM)
	/* Enable SMP mode for CPU0, by setting bit 6 of Auxiliary Ctl reg */
	asm volatile(
		"mrc p15, 0, r0, c1, c0, 1\n"
		"orr r0, r0, #1 << 6\n"
		"mcr p15, 0, r0, c1, c0, 1\n"
		::: "r0");
#endif

	clock_init();
	timer_init();
	gpio_init();

	eth_init_board();
}


/* The sunxi internal brom will try to loader external bootloader
 * from mmc0, nand flash, mmc2.
 */
uint32_t sunxi_get_boot_device(void)
{
	int boot_source;

	/*
	 * When booting from the SD card or NAND memory, the "eGON.BT0"
	 * signature is expected to be found in memory at the address 0x0004
	 * (see the "mksunxiboot" tool, which generates this header).
	 *
	 * When booting in the FEL mode over USB, this signature is patched in
	 * memory and replaced with something else by the 'fel' tool. This other
	 * signature is selected in such a way, that it can't be present in a
	 * valid bootable SD card image (because the BROM would refuse to
	 * execute the SPL in this case).
	 *
	 * This checks for the signature and if it is not found returns to
	 * the FEL code in the BROM to wait and receive the main u-boot
	 * binary over USB. If it is found, it determines where SPL was
	 * read from.
	 */
	if (!is_boot0_magic(SPL_ADDR + 4)) /* eGON.BT0 */
		return BOOT_DEVICE_BOARD;

	boot_source = readb(IOMEM_ADDR(SPL_ADDR + 0x28));
	switch (boot_source) {
	case SUNXI_BOOTED_FROM_MMC0:
		return BOOT_DEVICE_MMC1;
	case SUNXI_BOOTED_FROM_NAND:
		return BOOT_DEVICE_NAND;
	case SUNXI_BOOTED_FROM_MMC2:
		return BOOT_DEVICE_MMC2;
	case SUNXI_BOOTED_FROM_SPI:
		return BOOT_DEVICE_SPI;
	}

	panic("Unknown boot source %d\n", boot_source);
	return -1;		/* Never reached */
}

__weak void reset_cpu(void)
{

}

void *board_fdt_blob_setup(int *err)
{
	/* check u-boot with dtb.bin*/
	if (fdt_check_header(&_end) == 0)
		return (void *)&_end;

	/* uboot base + 2M offset */
	return (void *)(CONFIG_TEXT_BASE + SUNXI_DTB_OFFSET);
}


#if !CONFIG_IS_ENABLED(SYS_DCACHE_OFF) && !defined(CONFIG_ARM64)
void enable_caches(void)
{
	/* Enable D-cache. I-cache is already enabled in start.S */
	dcache_enable();
}
#endif

int setup_mon_len(void)
{
	gd->mon_len = (ulong)&__bss_end - (ulong)__image_copy_start;
	pr_debug("sunxi mon_len=0x%lx", gd->mon_len);
	return 0;
}
