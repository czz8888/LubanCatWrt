/*
 * (C) Copyright 2022-2025
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * lujianliang <lujianliang@allwinnertech.com>
 * SPDX-License-Identifier:     GPL-2.0+
 */

#include <common.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <malloc.h>
#include <memalign.h>
#include <spi.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <asm/arch/clock.h>
#include <asm/arch/sys_proto.h>
#include <asm/arch/gpio.h>
#include <cpu_func.h>
#include <linux/iopoll.h>
#include <sunxi_board.h>
#include "spif-sunxi.h"
#include <sys_config.h>
#include <fdt_support.h>
#include <linux/mtd/spi-nor.h>
#include <private_boot0.h>
#include <private_toc.h>
#include <linux/sizes.h>
#include <boot_param.h>
#include "../mtd/spi/sf_internal.h"

#define SUNXI_SPIF_DEV_NAME		"sunxi-spif"
#define SUNXI_SPIF_MODULE_VERSION       "1.0.0"

#define ROUND_UP(a, b) (((a) + (b)-1) & ~((b)-1))

#define	SUNXI_SPI_MAX_TIMEOUT	1000000
#define	SUNXI_SPI_PORT_OFFSET	0x1000
#define SUNXI_SPI_DEFAULT_CLK  (40000000)

/* For debug */
#define SPIF_DEBUG 0

#if SPIF_DEBUG
#define SPIF_EXIT()		printf("%s()%d - %s\n", __func__, __LINE__, "Exit")
#define SPIF_ENTER()		printf("%s()%d - %s\n", __func__, __LINE__, "Enter ...")
#define SPIF_DBG(fmt, arg...)	printf("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SPIF_INF(fmt, arg...)	printf("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SPIF_ERR(fmt, arg...)	printf("%s()%d - "fmt, __func__, __LINE__, ##arg)

#else
#define SPIF_EXIT()		pr_debug("%s()%d - %s\n", __func__, __LINE__, "Exit")
#define SPIF_ENTER()		pr_debug("%s()%d - %s\n", __func__, __LINE__, "Enter ...")
#define SPIF_DBG(fmt, arg...)	pr_debug("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SPIF_INF(fmt, arg...)	pr_debug("%s()%d - "fmt, __func__, __LINE__, ##arg)
#define SPIF_ERR(fmt, arg...)	printf("%s()%d - "fmt, __func__, __LINE__, ##arg)
#endif
#define SUNXI_SPIF_OK   0
#define SUNXI_SPIF_FAIL -1

struct sunxi_spif *g_sspif;
static int double_clk_flag;

extern int fdt_getprop_u32(const void *fdt, int nodeoffset,
				const char *prop, uint32_t *val);
static int sunxi_spif_hw_init(struct sunxi_spif *sspi);
static int sunxi_spif_set_speed(struct udevice *dev, uint speed);

struct sunxi_spif *get_sspif(void)
{
	return g_sspif;
}

#if SPIF_DEBUG
int spif_debug_flag;
static void spif_print_info(struct sunxi_spif *sspi)
{
	char buf[1024] = {0};
	void *base_addr = (void *)sspi->base_addr;

	snprintf(buf, sizeof(buf)-1,
			"sspi->base_addr = 0x%x, the SPIF control register:\n"
			"[VER] 0x%02x = 0x%08x, [GC]  0x%02x = 0x%08x, [GCA] 0x%02x = 0x%08x\n"
			"[TCR] 0x%02x = 0x%08x, [TDS] 0x%02x = 0x%08x, [INT] 0x%02x = 0x%08x\n"
			"[STA] 0x%02x = 0x%08x, [CSD] 0x%02x = 0x%08x, [PHC] 0x%02x = 0x%08x\n"
			"[TCF] 0x%02x = 0x%08x, [TCS] 0x%02x = 0x%08x, [TNM] 0x%02x = 0x%08x\n"
			"[PSR] 0x%02x = 0x%08x, [PSA] 0x%02x = 0x%08x, [PEA] 0x%02x = 0x%08x\n"
			"[PMA] 0x%02x = 0x%08x, [DMA] 0x%02x = 0x%08x, [DSC] 0x%02x = 0x%08x\n"
			"[DFT] 0x%02x = 0x%08x, [CFT] 0x%02x = 0x%08x, [CFS] 0x%02x = 0x%08x\n"
			"[BAT] 0x%02x = 0x%08x, [BAC] 0x%02x = 0x%08x, [TB]  0x%02x = 0x%08x\n"
			"[RB]  0x%02x = 0x%08x\n",
			(unsigned int)sspi->base_addr,
			SPIF_VER_REG, readl(base_addr + SPIF_VER_REG),
			SPIF_GC_REG, readl(base_addr + SPIF_GC_REG),
			SPIF_GCA_REG, readl(base_addr + SPIF_GCA_REG),

			SPIF_TC_REG, readl(base_addr + SPIF_TC_REG),
			SPIF_TDS_REG, readl(base_addr + SPIF_TDS_REG),
			SPIF_INT_EN_REG, readl(base_addr + SPIF_INT_EN_REG),

			SPIF_INT_STA_REG, readl(base_addr + SPIF_INT_STA_REG),
			SPIF_CSD_REG, readl(base_addr + SPIF_CSD_REG),
			SPIF_PHC_REG, readl(base_addr + SPIF_PHC_REG),

			SPIF_TCF_REG, readl(base_addr + SPIF_TCF_REG),
			SPIF_TCS_REG, readl(base_addr + SPIF_TCS_REG),
			SPIF_TNM_REG, readl(base_addr + SPIF_TNM_REG),

			SPIF_PS_REG, readl(base_addr + SPIF_PS_REG),
			SPIF_PSA_REG, readl(base_addr + SPIF_PSA_REG),
			SPIF_PEA_REG, readl(base_addr + SPIF_PEA_REG),

			SPIF_PMA_REG, readl(base_addr + SPIF_PMA_REG),
			SPIF_DMA_CTL_REG, readl(base_addr + SPIF_DMA_CTL_REG),
			SPIF_DSC_REG, readl(base_addr + SPIF_DSC_REG),

			SPIF_DFT_REG, readl(base_addr + SPIF_DFT_REG),
			SPIF_CFT_REG, readl(base_addr + SPIF_CFT_REG),
			SPIF_CFS_REG, readl(base_addr + SPIF_CFS_REG),

			SPIF_BAT_REG, readl(base_addr + SPIF_BAT_REG),
			SPIF_BAC_REG, readl(base_addr + SPIF_BAC_REG),
			SPIF_TB_REG, readl(base_addr + SPIF_TB_REG),

			SPIF_RB_REG, readl(base_addr + SPIF_RB_REG));
			printf("%s\n\n", buf);
}

void spif_print_descriptor(struct spif_descriptor_op *spif_op_list)
{
	char buf[512] = {0};
	struct spif_descriptor_op *spif_op = spif_op_list;

print:
	snprintf(buf, sizeof(buf)-1,
			"hburst_rw_flag        : 0x%x\n"
			"block_data_len        : 0x%x\n"
			"data_addr             : 0x%x\n"
			"next_des_addr         : 0x%x\n"
			"trans_phase	       : 0x%x\n"
			"flash_addr	       : 0x%x\n"
			"cmd_mode_buswidth     : 0x%x\n"
			"addr_dummy_data_count : 0x%x\n",
			spif_op->hburst_rw_flag,
			spif_op->block_data_len,
			spif_op->data_addr,
			spif_op->next_des_addr,
			spif_op->trans_phase,
			spif_op->flash_addr,
			spif_op->cmd_mode_buswidth,
			spif_op->addr_dummy_data_count);
			printf("%s", buf);
	printf("spif_op addr [%x]\n\n", (u32)spif_op);

	if (spif_op->next_des_addr) {
#ifdef CONFIG_MACH_SUN8IW21
		spif_op = (struct spif_descriptor_op *)(spif_op->next_des_addr);
#else
		spif_op = (struct spif_descriptor_op *)(spif_op->next_des_addr << 2);
#endif
		goto print;
	}

}

#endif

u32 sunxi_spif_get_version(void)
{
	return readl((void *)g_sspif->base_addr + SPIF_VER_REG);
}

static s32 sunxi_get_spif_mode(void)
{
	int nodeoffset = 0;
	int ret = 0;
	u32 rval = 0;
	u32 mode = 0;

	nodeoffset =  fdt_path_offset(working_fdt, "spif/spif-nor");
	if (nodeoffset < 0) {
		SPIF_INF("get spif para fail\n");
		return -1;
	}

	ret = fdt_getprop_u32(working_fdt, nodeoffset, "spif-rx-bus-width",
			(uint32_t *)(&rval));
	if (ret < 0) {
		SPIF_INF("get spif-rx-bus-width fail %d\n", ret);
		return -2;
	}

	if (rval == 2) {
		mode |= SPI_RX_DUAL;
	} else if (rval == 4) {
		mode |= SPI_RX_QUAD;
	} else if (rval == 8) {
		mode |= SPI_RX_OCTAL;
	} else
		mode |= SPI_RX_SLOW;

	ret = fdt_getprop_u32(working_fdt, nodeoffset, "spif-tx-bus-width",
			(uint32_t *)(&rval));
	if (ret < 0) {
		SPIF_INF("get spif-tx-bus-width fail %d\n", ret);
		return -3;
	}

	if (rval == 4) {
		mode |= SPI_TX_QUAD;
	} else if (rval == 8) {
		mode |= SPI_TX_OCTAL;
	} else
		mode |= SPI_TX_BYTE;

	ret = fdt_getprop_u32(working_fdt, nodeoffset, "dtr_mode_enabled",
			(uint32_t *)(&rval));
	if (ret < 0) {
		SPIF_ERR("get dtr_mode_enable fail %d\n", ret);
	} else {
		if (rval)
			mode |= SPI_DTR_MODE;
	}

	ret = fdt_getprop_u32(working_fdt, nodeoffset, "io_mode_enabled",
			(uint32_t *)(&rval));
	if (ret < 0) {
		SPIF_ERR("get io_mode_enable fail %d\n", ret);
	} else {
		if (rval)
			mode |= SPI_IO_MODE;
	}

	return mode;
}

static void spif_big_little_endian(bool endian, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	if (endian == LSB_FIRST)
		reg_val |= (SPIF_GC_RX_CFG_FBS | SPIF_GC_TX_CFG_FBS);
	else
		reg_val &= ~(SPIF_GC_RX_CFG_FBS | SPIF_GC_TX_CFG_FBS);
	writel(reg_val, base_addr + SPIF_GC_REG);
}

static void spif_clean_mode_en(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	reg_val &= ~(SPIF_GC_NMODE_EN | SPIF_GC_PMODE_EN);
	writel(reg_val, base_addr + SPIF_GC_REG);
}

static void spif_wp_en(bool enable, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	if (enable)
		reg_val |= SPIF_GC_WP_EN;
	else
		reg_val &= ~SPIF_GC_WP_EN;
	writel(reg_val, base_addr + SPIF_GC_REG);
}

static void spif_hold_en(bool enable, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	if (enable)
		reg_val |= SPIF_GC_HOLD_EN;
	else
		reg_val &= ~SPIF_GC_HOLD_EN;
	writel(reg_val, base_addr + SPIF_GC_REG);
}

static void spif_set_cs_pol(bool pol, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	if (pol)
		reg_val |= SPIF_GC_CS_POL;
	else
		reg_val &= ~SPIF_GC_CS_POL;
	writel(reg_val, base_addr + SPIF_GC_REG);
}

/* spif config chip select */
static s32 spif_set_cs(u32 chipselect, void __iomem *base_addr)
{
	int ret;
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	if (chipselect < 4) {
		reg_val &= ~SPIF_GC_SS_MASK;/* SS-chip select, clear two bits */
		reg_val |= chipselect << SPIF_GC_SS_BIT_POS;/* set chip select */
		reg_val |= SPIF_GC_CS_POL;/* active low polarity */
		writel(reg_val, base_addr + SPIF_GC_REG);
		ret = SUNXI_SPIF_OK;
	} else {
		SPIF_ERR("Chip Select set fail! cs = %d\n", chipselect);
		ret = SUNXI_SPIF_FAIL;
	}

	return ret;
}

static void spif_set_mode(u32 spi_mode, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	reg_val &= ~SPIF_MASK;
	reg_val |= spi_mode;
	writel(reg_val, base_addr + SPIF_GC_REG);
}

void spif_samp_dl_sw_rx_status(void __iomem  *base_addr, unsigned int status)
{
	unsigned int rval = readl(base_addr + SPIF_TC_REG);

	if (status)
		rval |= SPIF_ANALOG_DL_SW_RX_EN;
	else
		rval &= ~SPIF_ANALOG_DL_SW_RX_EN;

	writel(rval, base_addr +SPIF_TC_REG);
}

static void spif_samp_mode(void __iomem  *base_addr, unsigned int status)
{
	unsigned int rval = readl(base_addr + SPIF_TC_REG);

	if (status)
		rval |= SPIF_DIGITAL_ANALOG_EN;
	else
		rval &= ~SPIF_DIGITAL_ANALOG_EN;

	writel(rval, base_addr + SPIF_TC_REG);
}

static void spif_set_sample_mode(void __iomem *base_addr, unsigned int mode)
{
	unsigned int rval = readl(base_addr + SPIF_TC_REG);

	rval &= (~SPIF_DIGITAL_DELAY_MASK);
	rval |= mode << SPIF_DIGITAL_DELAY;
	writel(rval, base_addr + SPIF_TC_REG);
}

static void spif_set_sample_delay(void __iomem  *base_addr,
		unsigned int sample_delay)
{
	unsigned int rval = readl(base_addr + SPIF_TC_REG);

	rval &= (~SPIF_ANALOG_DELAY_MASK);
	rval |= sample_delay << SPIF_ANALOG_DELAY;
	writel(rval, base_addr + SPIF_TC_REG);
	mdelay(1);
}

static void spif_config_tc(struct sunxi_spif *sspi)
{
	void __iomem *base_addr = sspi->base_addr;

	if (sspi->sample_mode != SAMP_MODE_DL_DEFAULT) {
		spif_samp_mode(base_addr, 1);
		spif_samp_dl_sw_rx_status(base_addr, 1);
		spif_set_sample_mode(base_addr, sspi->sample_mode);
		spif_set_sample_delay(base_addr, sspi->sample_delay);
	}
}
/*
static void spif_set_dqs(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_DFT_REG);

	reg_val |= SPIF_DFT_DQS;
	writel(reg_val, base_addr + SPIF_DFT_REG);
}

static void spif_set_cdc(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_CFT_REG);

	reg_val = SPIF_CFT_CDC;
	writel(reg_val, base_addr + SPIF_CFT_REG);
}
*/
static void spif_set_csd(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_CSD_REG);

	reg_val |= SPIF_CSD_DEF;
	writel(reg_val, base_addr + SPIF_CSD_REG);
}

/* soft reset spif controller */
static void spif_soft_reset_fifo(void __iomem *base_addr)
{
	unsigned int timeout = 0xfffff;
	u32 reg_val = readl(base_addr + SPIF_GCA_REG);

	writel(reg_val | SPIF_DMA_END, base_addr + SPIF_GCA_REG);
	while (readl(base_addr + SPIF_GCA_REG) & SPIF_DMA_END) {
		timeout--;
		if (!timeout)
			printf("SPIF dma reset time_out\n");
	}

	timeout = 0xfffff;

	writel(reg_val | SPIF_GCA_SRST, base_addr + SPIF_GCA_REG);
	while (readl(base_addr + SPIF_GCA_REG) & SPIF_GCA_SRST) {
		timeout--;
		if (!timeout)
			printf("SPIF soft reset time_out\n");
	}

	return;
}

static void spif_reset_fifo(void __iomem *base_addr)
{
	unsigned int timeout = 0xfffff;
	u32 reg_val = readl(base_addr + SPIF_GCA_REG);

	reg_val |= SPIF_FIFO_SRST;

	writel(reg_val, base_addr + SPIF_GCA_REG);

	while (readl(base_addr + SPIF_GCA_REG) & SPIF_FIFO_SRST) {
		timeout--;
		if (!timeout) {
			printf("SPIF fifo reset time_out\n");
			return;
		}
	}
}

static void spif_set_trans_mode(u8 mode, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	if (mode)
		reg_val |= SPIF_GC_CFG_MODE;
	else
		reg_val &= ~SPIF_GC_CFG_MODE;
	writel(reg_val, base_addr + SPIF_GC_REG);
}

/* set first descriptor start addr */
static void spif_set_des_start_addr(struct spif_descriptor_op *spif_op,
				void __iomem *base_addr)
{
#ifdef CONFIG_MACH_SUN8IW21
	writel((u32)spif_op, base_addr + SPIF_DSC_REG);
#else
	/* addr word alignment */
	writel((u32)spif_op >> 2, base_addr + SPIF_DSC_REG);
#endif
}

/* set descriptor len */
static void spif_set_des_len(int len, void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_DMA_CTL_REG);

	reg_val |= len;
	writel(reg_val, base_addr + SPIF_DMA_CTL_REG);
}

/* DMA start Signal */
static void spif_dma_start_signal(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_DMA_CTL_REG);

	reg_val |= CFG_DMA_START;
	writel(reg_val, base_addr + SPIF_DMA_CTL_REG);
}

static void spif_trans_type_enable(u32 type_phase, void __iomem *base_addr)
{
	writel(type_phase, base_addr + SPIF_PHC_REG);
}

static void spif_set_flash_addr(u32 flash_addr, void __iomem *base_addr)
{
	writel(flash_addr, base_addr + SPIF_TCF_REG);
}

static void spif_set_buswidth(u32 cmd_mode_buswidth, void __iomem *base_addr)
{
	writel(cmd_mode_buswidth, base_addr + SPIF_TCS_REG);
}

static void spif_set_data_count(u32 addr_dummy_data_count, void __iomem *base_addr)
{
	writel(addr_dummy_data_count, base_addr + SPIF_TNM_REG);
}

static void spif_cpu_start_transfer(void __iomem *base_addr)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	reg_val |= SPIF_GC_NMODE_EN;
	writel(reg_val, base_addr + SPIF_GC_REG);
}

static void spif_set_output_clk(void __iomem *base_addr, u32 status)
{
	u32 reg_val = readl(base_addr + SPIF_TC_REG);

	if (status)
		reg_val |= SPIF_CLK_SCKOUT_SRC_SEL;
	else
		reg_val &= ~SPIF_CLK_SCKOUT_SRC_SEL;
	writel(reg_val, base_addr + SPIF_TC_REG);
}

static void spif_set_dtr(void __iomem *base_addr, u32 status)
{
	u32 reg_val = readl(base_addr + SPIF_GC_REG);

	if (status)
		reg_val |= SPIF_GC_DTR_EN;
	else
		reg_val &= ~SPIF_GC_DTR_EN;
	writel(reg_val, base_addr + SPIF_GC_REG);
}

static int sunxi_spif_clk_exit(struct sunxi_spif *sspi)
{
	clk_disable(&sspi->clk_mod);

	return 0;
}

void spif_init_clk(struct sunxi_spif *sspi)
{
	int ret = 0, nodeoffset = 0;
	u32 rval = 0;

	nodeoffset =  fdt_path_offset(working_fdt, "spif/spif-nor");
	if (nodeoffset < 0) {
		SPIF_ERR("get spi0 para fail\n");
	} else {
		ret = fdt_getprop_u32(working_fdt, nodeoffset,
				"spif-max-frequency", (uint32_t *)(&rval));
		if (ret < 0) {
			SPIF_ERR("get spif-max-frequency fail %d\n", ret);
		} else
			sspi->speed_hz = rval;
	}

	dev_info(sspi->dev, "spif sspi->speed_hz:%d \n", sspi->speed_hz);
	/* clock */
	if (sunxi_spif_set_speed(sspi->dev, sspi->speed_hz))
		SPIF_ERR("spi clk init error\n");

	double_clk_flag = 0;
	return;
}

static void spif_dtr_enable(struct sunxi_spif *sspi,
		struct spif_descriptor_op *spif_op)
{
	void __iomem *base_addr = (void *)(ulong)sspi->base_addr;
	unsigned int clk = sspi->speed_hz;
	unsigned int dtr_double_clk = clk * 2;

	if (!sspi->rx_dtr_en && !sspi->tx_dtr_en)
		return;

	if ((spif_op->cmd_mode_buswidth >> SPIF_ADDR_TRANS_POS) & 0x3) {
		if ((spif_op->trans_phase & SPIF_RX_TRANS_EN) &&
				sspi->rx_dtr_en) {
			spif_set_output_clk(base_addr, 1);
			spif_set_dtr(base_addr, 1);
			if (!double_clk_flag) {
				sunxi_spif_set_speed(sspi->dev, dtr_double_clk);
				double_clk_flag = 1;
			}
		} else if (spif_op->trans_phase & SPIF_TX_TRANS_EN &&
				sspi->tx_dtr_en) {
			spif_set_output_clk(base_addr, 1);
			spif_set_dtr(base_addr, 1);
			if (!dtr_double_clk) {
				sunxi_spif_set_speed(sspi->dev, dtr_double_clk);
				double_clk_flag = 1;
			}
		}
	} else {
		spif_set_output_clk(base_addr, 0);
		spif_set_dtr(base_addr, 0);
		if (double_clk_flag) {
			sunxi_spif_set_speed(sspi->dev, clk);
			double_clk_flag = 0;
		}
	}
}

void spif_init_dtr(u32 flags)
{
	struct sunxi_spif *sspi = get_sspif();

	if ((sspi->mode & SPI_DTR_MODE) && (flags & USE_RX_DTR))
		sspi->rx_dtr_en = 1;
	else
		sspi->rx_dtr_en = 0;

	if ((sspi->mode & SPI_DTR_MODE) && (flags & USE_TX_DTR))
		sspi->tx_dtr_en = 1;
	else
		sspi->tx_dtr_en = 0;
}

static void spif_ctr_recover(struct sunxi_spif *sspi)
{
	sunxi_spif_clk_exit(sspi);
	sunxi_spif_hw_init(sspi);
}

void spif_update_right_delay_para(struct mtd_info *mtd)
{
	struct spi_nor *nor = mtd->priv;
	struct spi_slave *slave = nor->spi;
	struct sunxi_spif *sspi = dev_get_priv(slave->dev->parent);
	void __iomem *base_addr = (void *)(ulong)sspi->base_addr;
	unsigned int sample_delay;
	unsigned int start_ok = 0, end_ok = 0, len_ok = 0, mode_ok = 0;
	unsigned int start_backup = 0, end_backup = 0, len_backup = 0;
	unsigned int mode = 0, startry_mode = 0, endtry_mode = 2, block = 0;
	u8 erase_opcode = nor->erase_opcode;
	uint32_t erasesize = mtd->erasesize;
	if (mtd->size > SZ_16M)
		nor->erase_opcode = SPINOR_OP_BE_32K_4B;
	else
		nor->erase_opcode = SPINOR_OP_BE_32K;
	mtd->erasesize = 32 * 1024;

	size_t retlen;
	u_char *cache_source;
	u_char *cache_target;
	u_char *cache_boot0;
	int len = nor->page_size;

	struct erase_info instr;
	instr.addr = block * mtd->erasesize;
	instr.len = (endtry_mode - startry_mode + 1) * mtd->erasesize;

	cache_boot0 = memalign(CONFIG_SYS_CACHELINE_SIZE, instr.len);
	mtd->_read(mtd, instr.addr, instr.len, &retlen, cache_boot0);
	mtd->_erase(mtd, &instr);

	cache_source = memalign(CONFIG_SYS_CACHELINE_SIZE, len);;
	cache_target = memalign(CONFIG_SYS_CACHELINE_SIZE, len);;
	memset(cache_source, 0xa5, len);

	/* re-initialize from device tree */
	spif_init_clk(sspi);

	spif_samp_mode(base_addr, 1);
	spif_samp_dl_sw_rx_status(base_addr, 1);
	for (mode = startry_mode; mode <= endtry_mode; mode++) {
		sspi->sample_mode = mode;
		spif_set_sample_mode(base_addr, mode);
		for (sample_delay = 0; sample_delay < 64; sample_delay++) {
			spif_set_sample_delay(base_addr, sample_delay);
			mtd->_write(mtd, block * mtd->erasesize +
					sample_delay * nor->page_size,
					len, &retlen, cache_source);
		}

		for (sample_delay = 0; sample_delay < 64; sample_delay++) {
			sspi->sample_delay = sample_delay;
			spif_set_sample_delay(base_addr, sample_delay);
			memset(cache_target, 0, len);
			mtd->_read(mtd, block * mtd->erasesize +
					sample_delay * nor->page_size,
					len, &retlen, cache_target);

			if (strncmp((char *)cache_source, (char *)cache_target,
						len) == 0) {
				dev_dbg(sspi->dev, "mode:%d delat:%d [OK]\n",
						mode, sample_delay);
				if (!len_backup) {
					start_backup = sample_delay;
					end_backup = sample_delay;
				} else
					end_backup = sample_delay;
				len_backup++;
			} else {
				dev_dbg(sspi->dev, "mode:%d delay:%d [ERROR]\n",
						mode, sample_delay);
				if (!start_backup)
					continue;
				else {
					if (len_backup > len_ok) {
						len_ok = len_backup;
						start_ok = start_backup;
						end_ok = end_backup;
						mode_ok = mode;
					}

					len_backup = 0;
					start_backup = 0;
					end_backup = 0;
				}
			}
		}

		if (len_backup > len_ok) {
			len_ok = len_backup;
			start_ok = start_backup;
			end_ok = end_backup;
			mode_ok = mode;
		}
		len_backup = 0;
		start_backup = 0;
		end_backup = 0;

		block++;
	}

	if (!len_ok) {
		sspi->sample_delay = SAMP_MODE_DL_DEFAULT;
		sspi->sample_mode = SAMP_MODE_DL_DEFAULT;
		spif_samp_mode(base_addr, 0);
		spif_samp_dl_sw_rx_status(base_addr, 0);
		sspi->speed_hz = 25000000;

		/* clock */
		if (sunxi_spif_set_speed(sspi->dev, sspi->speed_hz))
			pr_err("spi clk init error\n");

		dev_err(sspi->dev, "spif update delay param error\n");
	} else {
		sspi->sample_delay = (start_ok + end_ok) / 2;
		sspi->sample_mode = mode_ok;
		spif_set_sample_delay(base_addr, sspi->sample_delay);
		spif_set_sample_mode(base_addr, sspi->sample_mode);
	}
	dev_info(sspi->dev, "Sample mode:%d start:%d end:%d sample_delay:0x%x\n",
			mode_ok, start_ok, end_ok,
			sspi->sample_delay);

	mtd->_erase(mtd, &instr);
	mtd->_write(mtd, instr.addr, instr.len, &retlen, cache_boot0);

	nor->erase_opcode = erase_opcode;
	mtd->erasesize = erasesize;
	kfree(cache_source);
	kfree(cache_target);
	kfree(cache_boot0);
	return ;
}

static void spif_boot_try_delay_param(struct mtd_info *mtd,
				boot_spinor_info_t *boot_info)
{
	struct spi_nor *nor = mtd->priv;
	struct spi_slave *slave = nor->spi;
	struct sunxi_spif *sspi = dev_get_priv(slave->dev->parent);
	void __iomem *base_addr = (void *)(ulong)sspi->base_addr;
	unsigned int sample_delay;
	unsigned int start_ok = 0, end_ok = 0, len_ok = 0, mode_ok = 0;
	unsigned int start_backup = 0, end_backup = 0, len_backup = 0;
	unsigned int mode = 0, startry_mode = 0, endtry_mode = 2;
	size_t retlen, len = 512;
	boot0_file_head_t *boot0_head;
	boot0_head = memalign(CONFIG_SYS_CACHELINE_SIZE, len);

	/* re-initialize from device tree */
	spif_init_clk(sspi);

	spif_samp_mode(base_addr, 1);
	spif_samp_dl_sw_rx_status(base_addr, 1);
	for (mode = startry_mode; mode <= endtry_mode; mode++) {
		sspi->sample_mode = mode;
		spif_set_sample_mode(base_addr, mode);
		for (sample_delay = 0; sample_delay < 64; sample_delay++) {
			sspi->sample_delay = sample_delay;
			spif_set_sample_delay(base_addr, sample_delay);
			memset(boot0_head, 0, len);
			mtd->_read(mtd, 0, len, &retlen, (u_char *)boot0_head);

			if (strncmp((char *)boot0_head->boot_head.magic,
				(char *)BOOT0_MAGIC,
				sizeof(boot0_head->boot_head.magic)) == 0) {
				dev_dbg(sspi->dev, "mode:%d delat:%d [OK]\n",
						mode, sample_delay);
				if (!len_backup) {
					start_backup = sample_delay;
					end_backup = sample_delay;
				} else
					end_backup = sample_delay;
				len_backup++;
			} else {
				dev_dbg(sspi->dev, "mode:%d delay:%d [ERROR]\n",
						mode, sample_delay);
				if (!start_backup)
					continue;
				else {
					if (len_backup > len_ok) {
						len_ok = len_backup;
						start_ok = start_backup;
						end_ok = end_backup;
						mode_ok = mode;
					}

					len_backup = 0;
					start_backup = 0;
					end_backup = 0;
				}
			}
		}
		if (len_backup > len_ok) {
			len_ok = len_backup;
			start_ok = start_backup;
			end_ok = end_backup;
			mode_ok = mode;
		}
		len_backup = 0;
		start_backup = 0;
		end_backup = 0;
	}

	if (!len_ok) {
		sspi->sample_delay = SAMP_MODE_DL_DEFAULT;
		sspi->sample_mode = SAMP_MODE_DL_DEFAULT;
		spif_samp_mode(base_addr, 0);
		spif_samp_dl_sw_rx_status(base_addr, 0);
		sspi->speed_hz = 25000000;
		/* clock */
		if (sunxi_spif_set_speed(sspi->dev, sspi->speed_hz))
			pr_err("spi clk init error\n");

		dev_err(sspi->dev, "spif update delay param error\n");
	} else {
		sspi->sample_delay = (start_ok + end_ok) / 2;
		sspi->sample_mode = mode_ok;
		spif_set_sample_delay(base_addr, sspi->sample_delay);
		spif_set_sample_mode(base_addr, sspi->sample_mode);
	}
	dev_info(sspi->dev, "Sample mode:%d start:%d end:%d sample_delay:0x%x\n",
			mode_ok, start_ok, end_ok,
			sspi->sample_delay);

	boot_info->sample_delay = sspi->sample_delay;
	boot_info->sample_mode = sspi->sample_mode;
	kfree(boot0_head);
	return;
}

extern int update_boot_param(void);
int spif_set_right_delay_para(struct mtd_info *mtd)
{
	struct spi_nor *nor = mtd->priv;
	struct spi_slave *slave = nor->spi;
	struct sunxi_spif *sspi = dev_get_priv(slave->dev->parent);
	void __iomem *base_addr = (void *)(ulong)sspi->base_addr;
	boot_spinor_info_t *boot_info = NULL;
	struct sunxi_boot_param_region *boot_param = gd->sunxi_boot_param_addr;

	if (!boot_param) {
		dev_err(sspi->dev, "%s() gd->boot_param is NULL\n", __func__);
		return -1;
	}

	boot_info = (boot_spinor_info_t *)boot_param->spiflash_info;
	if (strncmp((const char *)boot_param->header.magic,
				(const char *)BOOT_PARAM_MAGIC,
				sizeof(boot_param->header.magic)) ||
		strncmp((const char *)boot_info->magic,
				(const char *)SPINOR_BOOT_PARAM_MAGIC,
				sizeof(boot_info->magic))) {
		printf("boot param magic error: %s \n", boot_param->header.magic);
		spif_boot_try_delay_param(mtd, boot_info);
		if (update_boot_param())
			printf("update boot param error\n");
	}

	if (boot_info->sample_delay == SAMP_MODE_DL_DEFAULT) {
		printf("boot smple delay error\n");
		spif_boot_try_delay_param(mtd, boot_info);
		if (update_boot_param())
			printf("update boot param error\n");
	}

	dev_info(sspi->dev, "spi sample_mode:%x sample_delay:%x\n",
			boot_info->sample_mode, boot_info->sample_delay);

	spif_init_clk(sspi);

	spif_samp_mode(base_addr, 1);
	spif_samp_dl_sw_rx_status(base_addr, 1);
	spif_set_sample_mode(base_addr, boot_info->sample_mode);
	spif_set_sample_delay(base_addr, boot_info->sample_delay);
	sspi->sample_delay = boot_info->sample_delay;
	sspi->sample_mode = boot_info->sample_mode;

	return 0;
}

int spif_xfer(struct spi_slave *slave, struct spif_descriptor_op *spif_op,
		unsigned int data_len)
{
	int timeout = 0xfffffff;
	struct sunxi_spif *sspi = dev_get_priv(slave->dev->parent);

	void __iomem *base_addr =
		(void __iomem *)(unsigned long)sspi->base_addr;
	uint desc_count = ((data_len + SPIF_MAX_TRANS_NUM - 1) / SPIF_MAX_TRANS_NUM) + 1;
	uint desc_size = desc_count * sizeof(struct spif_descriptor_op);
#ifdef CONFIG_MACH_SUN8IW21
	unsigned int data_addr = (u32)spif_op->data_addr;
#else
	unsigned int data_addr = (u32)spif_op->data_addr << 2;
#endif

	spif_reset_fifo(base_addr);
	spif_dtr_enable(sspi, spif_op);
	if ((spif_op->block_data_len & DMA_DATA_LEN) == 0) {
		spif_set_trans_mode(SPIF_GC_CPU_MODE, base_addr);

		spif_trans_type_enable(spif_op->trans_phase, base_addr);

		spif_set_flash_addr(spif_op->flash_addr, base_addr);

		spif_set_buswidth(spif_op->cmd_mode_buswidth, base_addr);

		spif_set_data_count(spif_op->addr_dummy_data_count, base_addr);

		spif_cpu_start_transfer(base_addr);

		while ((readl(base_addr + SPIF_GC_REG) & SPIF_GC_NMODE_EN)) {
			timeout--;
			if (!timeout) {
				printf("SPIF DMA transfer time_out\n");
				spif_ctr_recover(sspi);
				return -1;
			}
		}
#if SPIF_DEBUG
		if (spif_debug_flag)
			spif_print_info(sspi);
#endif
	} else {
#if 0 // CONFIG_SPI_USE_DMA
		spif_set_trans_mode(SPIF_GC_CPU_MODE, base_addr);

		spif_trans_type_enable(spif_op->trans_phase, base_addr);

		spif_set_flash_addr(spif_op->flash_addr, base_addr);

		spif_set_buswidth(spif_op->cmd_mode_buswidth, base_addr);

		spif_set_data_count(spif_op->addr_dummy_data_count, base_addr);
#else
		spif_set_trans_mode(SPIF_GC_DMA_MODE, base_addr);
#endif
		/* flush data addr */
		flush_cache(data_addr, ROUND_UP(data_len, CONFIG_SYS_CACHELINE_SIZE));
		flush_cache((u32)spif_op, ROUND_UP(desc_size, CONFIG_SYS_CACHELINE_SIZE));

		spif_set_des_start_addr(spif_op, base_addr);

		spif_set_des_len(DMA_DESCRIPTOR_LEN, base_addr);

#if SPIF_DEBUG
		if (spif_debug_flag) {
			spif_print_descriptor(spif_op);
			spif_print_info(sspi);
		}
#endif
		spif_dma_start_signal(base_addr);

		/*
		 *  The SPIF move data through DMA, and DMA and CPU modes
		 *  differ only between actively configuring registers and
		 *  configuring registers through the DMA descriptor
		 */
#if 0 // CONFIG_SPI_USE_DMA
		spif_cpu_start_transfer(base_addr);
#endif

		/* waiting DMA finish */
		while (!(readl(base_addr + SPIF_INT_STA_REG) &
				DMA_TRANS_DONE_INT)) {
			timeout--;
			if (!timeout) {
				printf("SPIF DMA transfer time_out\n");
				spif_ctr_recover(sspi);
				return -1;
			}
		}

		invalidate_dcache_range(data_addr, data_addr +
				ROUND_UP(data_len, CONFIG_SYS_CACHELINE_SIZE));
		writel(DMA_TRANS_DONE_INT, base_addr + SPIF_INT_STA_REG);
	}

	return 0;
}

static int sunxi_spif_claim_bus(struct udevice *dev)
{
	struct sunxi_spif *sspi = get_sspif();
	void __iomem *base_addr = sspi->base_addr;

	/* 1. reset all tie logic & fifo */
	spif_soft_reset_fifo(base_addr);
	spif_clean_mode_en(base_addr);

	/* 2. interface first transmit bit select */
	spif_big_little_endian(MSB_FIRST, base_addr);

	/* 3. disable wp & hold */
	spif_wp_en(0, base_addr);
	spif_hold_en(0, base_addr);

	/* 4. disable DTR */
	spif_set_output_clk(base_addr, 0);
	spif_set_dtr(base_addr, 0);

	/* 5. set the default chip select */
	spif_set_cs(0, base_addr);
	spif_set_cs_pol(1, base_addr);

	/* 6. set spi CPOL and CPHA */
	spif_set_mode(SPIF_MODE0, base_addr);

	/* 7. set sample delay timing */
	spif_config_tc(sspi);

	/* 8. set reg defauld count */
	//spif_set_dqs(base_addr);
	//spif_set_cdc(base_addr);
	spif_set_csd(base_addr);

	//spif_set_sample_mode(base_addr, 1);

	return 0;
}

static int sunxi_spif_release_bus(struct udevice *dev)
{
	struct sunxi_spif *sspi = dev_get_priv(dev->parent);

	dev_info(sspi->dev, "%s()\n", __func__);

	return 0;
}

static int sunxi_spif_xfer(struct udevice *dev, unsigned int bitlen,
			  const void *dout, void *din, unsigned long flags)
{
	struct sunxi_spif *sspi = dev_get_priv(dev->parent);
	int num, len;

	len = bitlen / 8;
	num = DIV_ROUND_UP(len, SPIF_MAX_TRANS_NUM);
	dev_info(sspi->dev, "size %d splite to %d num xfer\n", len, num);

	return 0;
}

static int sunxi_spif_set_speed(struct udevice *dev, uint speed)
{
	struct sunxi_spif *sspi = dev_get_priv(dev);
	u32 old_clk = 0;
	int ret = 0;
	extern bool speed_lock;

	if (speed_lock)
		speed = SUNXI_SPIF_DEFAULT_FREQUENCY;

	old_clk = clk_get_rate(&sspi->clk_mod);
	if (old_clk == speed)
		return ret;

	if (speed < sspi->data->min_speed_hz || speed > sspi->data->max_speed_hz) {
		dev_err(sspi->dev, "speed %d not in range\n", speed);
		return -EINVAL;
	}

	ret = clk_disable(&sspi->clk_mod);
	ret = clk_set_rate(&sspi->clk_mod, speed);
	if (ret) {
		dev_err(sspi->dev, "set clk freq %d failed %d\n", speed, ret);
		clk_set_rate(&sspi->clk_mod, old_clk);
		ret = clk_enable(&sspi->clk_mod);
		return ret;
	}

	sspi->speed_hz = speed;
	ret = clk_enable(&sspi->clk_mod);
	return ret;
}

static int sunxi_spif_set_mode(struct udevice *dev, uint mode)
{
	struct sunxi_spif *sspi = dev_get_priv(dev);

	dev_info(sspi->dev, "sspi->mode:0x%x  mode:0x%x\n", sspi->mode, mode);

	return 0;
}

static const struct dm_spi_ops sunxi_spif_ops = {
	.claim_bus		= NULL,
	.release_bus		= sunxi_spif_release_bus,
	.xfer			= sunxi_spif_xfer,
	.set_speed		= sunxi_spif_set_speed,
	.set_mode		= sunxi_spif_set_mode,
};

static int sunxi_spif_resource_get(struct sunxi_spif *sspi)
{
	int ret = 0;

	sspi->bus_num = dev_seq(sspi->dev);
	sspi->base_addr = dev_read_addr_ptr(sspi->dev);

	ret = clk_get_by_name(sspi->dev, "pll", &sspi->clk_pll);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to get pll clk %d\n", ret);
		goto out;
	}
	ret = clk_get_by_name(sspi->dev, "mod", &sspi->clk_mod);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to get mod clk %d\n", ret);
		goto out;
	}
	ret = clk_get_by_name(sspi->dev, "bus", &sspi->clk_bus);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to get bus clk %d\n", ret);
		goto out;
	}

	ret = reset_get_by_index(sspi->dev, 0, &sspi->reset);
	if (ret < 0) {
		dev_err(sspi->dev, "failed to get reset %d\n", ret);
		goto out;
	}

	sspi->data->max_speed_hz = dev_read_u32_default(sspi->dev,
			"clock-frequency", SUNXI_SPIF_MAX_FREQUENCY);
	if (sspi->bus_freq < SUNXI_SPIF_MIN_FREQUENCY || sspi->bus_freq > SUNXI_SPIF_MAX_FREQUENCY) {
		dev_warn(sspi->dev, "frequency no in range, use default value %d\n", SUNXI_SPIF_MAX_FREQUENCY);
		sspi->bus_freq = SUNXI_SPIF_MAX_FREQUENCY;
	}

	/* Get sampling delay parameters */
	sspi->sample_mode = dev_read_u32_default(sspi->dev,
			"sample_mode", SAMP_MODE_DL_DEFAULT);
	if (sspi->sample_mode < 0) {
		dev_err(sspi->dev, "Failed to get sample mode\n");
		sspi->sample_mode = SAMP_MODE_DL_DEFAULT;
	}
	sspi->sample_delay = dev_read_u32_default(sspi->dev, "sample_delay", SAMP_MODE_DL_DEFAULT);
	if (sspi->sample_delay < 0) {
		dev_err(sspi->dev, "Failed to get sample delay\n");
		sspi->sample_delay = SAMP_MODE_DL_DEFAULT;
	}
	dev_info(sspi->dev, "sample_mode:%x sample_delay:%x\n",
				sspi->sample_mode, sspi->sample_delay);

	/* AW SPIF controller self working mode */
/*
	if (of_property_read_bool(np, "prefetch_read_mode_enabled")) {
		dev_info(sspi->dev, "prefetch read mode enabled");
		sspi->working_mode |= PREFETCH_READ_MODE;
	}
	if (of_property_read_bool(np, "dqs_mode_enabled")) {
		dev_info(sspi->dev, "DQS mode enabled");
		sspi->working_mode |= DQS;
	}
*/
out:
	return ret;
}

static int sunxi_spif_clk_init(struct sunxi_spif *sspi)
{
	int ret = 0;

	reset_assert(&sspi->reset);
	reset_deassert(&sspi->reset);

	ret = clk_enable(&sspi->clk_pll);
	if (ret) {
		dev_err(sspi->dev, "failed to enable clk_pll %d\n", ret);
		return ret;
	}

	ret = clk_set_parent(&sspi->clk_mod, &sspi->clk_pll);
	if (ret) {
		dev_err(sspi->dev, "failed set mclk parent to pclk %d\n", ret);
		return ret;
	}

	ret = clk_enable(&sspi->clk_mod);
	if (ret) {
		dev_err(sspi->dev, "failed to enable clk_mod %d\n", ret);
		return ret;
	}

	ret = clk_enable(&sspi->clk_bus);
	if (ret) {
		dev_err(sspi->dev, "failed to enable clk_bus %d\n", ret);
		goto err_bus;
	}

	return 0;
err_bus:
	clk_disable(&sspi->clk_mod);
	return ret;
}

/*
 * sunxi_spif_hw_init : config the spif controller's public configration
 * return 0 on success, reutrn err num on failed
 */
static int sunxi_spif_hw_init(struct sunxi_spif *sspi)
{
	int ret = 0;
	SPIF_ENTER();

	ret = sunxi_spif_clk_init(sspi);
	if (ret) {
		dev_err(sspi->dev, "sunxi_spif clk init error\n");
		goto err;
	}

	ret = sunxi_spif_claim_bus(sspi->dev);
err:
	return ret;
}

static int sunxi_spif_probe(struct udevice *dev)
{
	struct sunxi_spif *sspi = dev_get_priv(dev);
	int ret = 0;

	sspi->dev = dev;
	sspi->data = (struct sunxi_spif_hw_data *)dev_get_driver_data(dev);
	g_sspif = sspi;

	ret = sunxi_spif_resource_get(sspi);
	if (ret) {
		dev_err(sspi->dev, "failed to get spif resource %d\n", ret);
		goto err;
	}

	ret = sunxi_spif_hw_init(sspi);
	if (ret) {
		dev_err(sspi->dev, "failed to init spif hardware %d\n", ret);
		goto err;
	}

	sspi->speed_hz = SUNXI_SPIF_DEFAULT_FREQUENCY;
	sspi->sample_delay = SAMP_MODE_DL_DEFAULT;
	sspi->sample_mode = SAMP_MODE_DL_DEFAULT;
	sspi->rx_dtr_en = 0;
	sspi->tx_dtr_en = 0;
	sspi->mode = sunxi_get_spif_mode();
	sspi->data->max_speed_hz = sspi->bus_freq;

	sunxi_spif_set_speed(sspi->dev, sspi->speed_hz);

	dev_info(sspi->dev, "probe succeed (Version %s)\n", SUNXI_SPIF_MODULE_VERSION);
err:
	return ret;
}

static struct sunxi_spif_hw_data sunxi_spif_data_v1_2 = {
	.max_speed_hz	= SUNXI_SPIF_MAX_FREQUENCY,
	.min_speed_hz	= SUNXI_SPIF_MIN_FREQUENCY,
};

static const struct udevice_id sunxi_spif_ids[] = {
	/* 1918 */
	{ .compatible = "allwinner,sunxi-spif-v1.2", .data = (ulong)&sunxi_spif_data_v1_2 },
	{},
};

U_BOOT_DRIVER(sunxi_spif) = {
	.name		= SUNXI_SPIF_DEV_NAME,
	.id		= UCLASS_SPI,
	.of_match	= sunxi_spif_ids,
	.ops		= &sunxi_spif_ops,
	.priv_auto	= sizeof(struct sunxi_spif),
	.probe		= sunxi_spif_probe,
};

