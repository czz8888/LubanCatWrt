// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2024-2027
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Tom Cubie <huangrongcun@allwinnertech.com>
 */
#include <sunxi_board.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <private_uboot.h>
#include <sunxi_boot_param.h>
#include <asm/global_data.h>

#define UARTLCR	0xC
#define UARTHALT 0xA4
#define UARTLCR_DLAB	0x80 /* bit7 */
#define UARTDLL 0x0
#define UARTDLLM 0x4
#define UART_MEM_SIZE	0x1000
#define UARTHALT_AT_BUSY 0x2

#define SUNXI_DRAM_PARM_STORE2SRAM 0x40038

int sunxi_baudrate_get(void)
{
	uint32_t val;
	static int32_t sclk = -1;
	static int32_t baudrate = -1;
	uint32_t  uart_base = 0;
	uint32_t clk_src = 0, factor_m = 0, divisor = 0;

	uart_base = (SUNXI_UART0_BASE + uboot_spare_head.boot_data.uart_port * UART_MEM_SIZE);

	val = readl(uart_base + UARTHALT);
	val |= UARTHALT_AT_BUSY;
	writel(val, uart_base + UARTHALT);

	/*adaptive select sclk and baudrate*/
	val = readl(SUNXI_UART_CLK_REG);
	clk_src = (val >> 24) & 0x7;
	factor_m = (val & 0x1f) + 1;

	val = readl(uart_base + UARTLCR);
	val |= UARTLCR_DLAB;
	writel(val, uart_base + UARTLCR);

	divisor = (readl(uart_base + UARTDLL) & 0xFF) | (readl(uart_base + UARTDLLM) & 0xFF) << 8;
	val &= ~UARTLCR_DLAB;
	writel(val, uart_base + UARTLCR);

	val = readl(uart_base + UARTHALT);
	val &= ~UARTHALT_AT_BUSY;
	writel(val, uart_base + UARTHALT);

	if (clk_src == SUNXI_UART_CLK_SRC_SEL_HOSC) {
		sclk = 24000000 / factor_m;
		baudrate = sclk / (16 * divisor);
		if (baudrate == 115384)
			baudrate = 115200;
	} else if (clk_src == SUNXI_UART_CLK_SRC_PERI0_600M) {
		sclk = 600000000 / factor_m;
		baudrate = sclk / (16 * divisor);
	} else if (clk_src == SUNXI_UART_CLK_SRC_PERI0_480M) {
		sclk = 480000000 / factor_m;
		baudrate = sclk / (16 * divisor);
	}

	return baudrate;
}

int sunxi_update_ddrpara_to_sram(void)
{
	int i;
	uint32_t *dram_para = NULL;
	uint32_t *dram_para_sram_store = (void *)SUNXI_DRAM_PARM_STORE2SRAM;
	typedef_sunxi_boot_param *sunxi_boot_param = gd->sunxi_boot_param_addr;
	dram_para = (uint32_t *)sunxi_boot_param->ddr_info;
	for (i = MAX_DRAMPARA_SIZE - 1; i >= 0; i--) {
		dram_para_sram_store[i] = dram_para[i];
	}
	return 0;
}

int sunxi_get_active_boot0_id(void)
{
	uint32_t val = *(uint32_t *)(SUNXI_RTC_BASE + 0x304);
	if (val & (1 << 15)) {
		return (val >> 12) & 0x7;
	} else {
		return (val >> 28) & 0x7;
	}
}