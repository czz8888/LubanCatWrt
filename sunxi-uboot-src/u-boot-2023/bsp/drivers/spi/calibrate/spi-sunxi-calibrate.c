/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * SUNXI SPI Controller Calibrate Driver
 *
 */

#include <common.h>
#include <errno.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <dm/device-internal.h>
#include <cpu_func.h>
#include <linux/io.h>
#include "spi-sunxi-calibrate.h"

static const u32 sunxi_sample_mode[] = {
	0x100, /* SUNXI_SPI_SAMP_DELAY_CYCLE_0_0 */
	0x000, /* SUNXI_SPI_SAMP_DELAY_CYCLE_0_5 */
	0x010, /* SUNXI_SPI_SAMP_DELAY_CYCLE_1_0 */
	0x110, /* SUNXI_SPI_SAMP_DELAY_CYCLE_1_5 */
	0x101, /* SUNXI_SPI_SAMP_DELAY_CYCLE_2_0 */
	0x001, /* SUNXI_SPI_SAMP_DELAY_CYCLE_2_5 */
	0x011  /* SUNXI_SPI_SAMP_DELAY_CYCLE_3_0 */
};

void sunxi_spi_bus_sample_mode(struct sunxi_spi *sspi, u32 mode)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_GC_REG);
	u32 reg_old = reg_val;

	switch (mode) {
	case SUNXI_SPI_SAMP_TYPE_OLD:
		reg_val &= ~SUNXI_SPI_GC_MODE_SEL;
		break;
	case SUNXI_SPI_SAMP_TYPE_NEW:
		reg_val |= SUNXI_SPI_GC_MODE_SEL;
		break;
	}
	if (reg_val != reg_old)
		writel(reg_val, sspi->base_addr + SUNXI_SPI_GC_REG);
}

void sunxi_spi_set_sample_delay_sw(struct sunxi_spi *sspi, u32 status)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
	u32 reg_old = reg_val;

	if (status)
		reg_val |= SUNXI_SPI_SAMP_DL_SW_EN;
	else
		reg_val &= ~SUNXI_SPI_SAMP_DL_SW_EN;

	if (reg_val != reg_old)
		writel(reg_val, sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
}

void sunxi_spi_set_sample_mode(struct sunxi_spi *sspi, u32 mode)
{
	u32 reg_old, reg_new;
	u32 sdm, sdc, sdc1;

	reg_new = reg_old = readl(sspi->base_addr + SUNXI_SPI_TC_REG);

	sdm = (sunxi_sample_mode[mode] >> 8) & 0xf;
	sdc = (sunxi_sample_mode[mode] >> 4) & 0xf;
	sdc1 = (sunxi_sample_mode[mode] >> 0) & 0xf;

	if (sdm)
		reg_new |= SUNXI_SPI_TC_SDM;
	else
		reg_new &= ~SUNXI_SPI_TC_SDM;

	if (sdc)
		reg_new |= SUNXI_SPI_TC_SDC;
	else
		reg_new &= ~SUNXI_SPI_TC_SDC;

	if (sdc1)
		reg_new |= SUNXI_SPI_TC_SDC1;
	else
		reg_new &= ~SUNXI_SPI_TC_SDC1;

	if (reg_new != reg_old)
		writel(reg_new, sspi->base_addr + SUNXI_SPI_TC_REG);
}

void sunxi_spi_set_sample_delay(struct sunxi_spi *sspi, u32 sample_delay)
{
	u32 reg_val = readl(sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
	u32 reg_old = reg_val;
	reg_val &= ~SUNXI_SPI_SAMP_DL_SW;
	reg_val |= FIELD_PREP(SUNXI_SPI_SAMP_DL_SW, sample_delay);
	if (reg_val != reg_old)
		writel(reg_val, sspi->base_addr + SUNXI_SPI_SAMP_DL_REG);
}

void sunxi_spi_delay_chain_init(struct sunxi_spi *sspi)
{
	if ((sspi->bus_mode & (SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND)) &&
		(sspi->data->quirk_flag & NEW_SAMPLE_MODE)) {
		sunxi_spi_bus_sample_mode(sspi, SUNXI_SPI_SAMP_TYPE_NEW);
		if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL) {
			sunxi_spi_set_sample_mode(sspi, sspi->sample_mode);
			if (sspi->sample_delay >= 0) {
				sunxi_spi_set_sample_delay_sw(sspi, true);
				sunxi_spi_set_sample_delay(sspi, sspi->sample_delay);
			}
		}
	} else {
		sunxi_spi_bus_sample_mode(sspi, SUNXI_SPI_SAMP_TYPE_OLD);
		if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL)
			sunxi_spi_set_sample_mode(sspi, sspi->sample_mode);
	}
}

void sunxi_spi_set_delay_chain(struct sunxi_spi *sspi, u32 clk)
{
	u32 sample_mode;

	switch (sspi->bus_sample_mode) {
	case SUNXI_SPI_SAMP_MODE_AUTO:
		if ((sspi->bus_mode & (SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND)) &&
			(sspi->data->quirk_flag & NEW_SAMPLE_MODE)) {
			if (clk >= SUNXI_SPI_SAMP_HIGH_FREQ)
				sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_5;
			else
				sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
		} else {
			if (clk >= SUNXI_SPI_SAMP_HIGH_FREQ)
				sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_1_0;
			else if (clk <= SUNXI_SPI_SAMP_LOW_FREQ)
				sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			else
				sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_5;
		}
		if (sspi->sample_mode != sample_mode) {
			sunxi_spi_set_sample_mode(sspi, sample_mode);
			sspi->sample_mode = sample_mode;
		}
		break;
	case SUNXI_SPI_SAMP_MODE_MANUAL:
		break;
	default:
		dev_err(sspi->dev, "unsupport sample mode %d\n", sspi->bus_sample_mode);
	}
}

void sunxi_spi_resource_get_calibrate(struct sunxi_spi *sspi)
{
	sspi->sample_mode = dev_read_u32_default(sspi->dev, "sample_mode", SUNXI_SPI_SAMP_MODE_DL_DEFAULT);
	sspi->sample_delay = dev_read_u32_default(sspi->dev, "sample_delay", SUNXI_SPI_SAMP_MODE_DL_DEFAULT);
	if (sspi->sample_mode == SUNXI_SPI_SAMP_MODE_DL_DEFAULT && sspi->sample_delay == SUNXI_SPI_SAMP_MODE_DL_DEFAULT) {
		sspi->bus_sample_mode = SUNXI_SPI_SAMP_MODE_AUTO;
		sspi->sample_mode = sspi->sample_delay = 0;
	} else {
		sspi->bus_sample_mode = SUNXI_SPI_SAMP_MODE_MANUAL;
		if (sspi->data->quirk_flag & NEW_SAMPLE_MODE) {
			if (sspi->sample_mode > SUNXI_SPI_SAMP_DELAY_CYCLE_3_0) {
				dev_warn(sspi->dev, "sample mode %d over new delay cycle\n", sspi->sample_mode);
				sspi->sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			}
			if (sspi->sample_delay > SUNXI_SPI_SAMPLE_DELAY_CHAIN_MAX) {
				dev_warn(sspi->dev, "sample delay %d over new delay chain\n", sspi->sample_delay);
				sspi->sample_delay = SUNXI_SPI_SAMPLE_DELAY_CHAIN_MIN;
			}
		} else {
			if (sspi->sample_mode > SUNXI_SPI_SAMP_DELAY_CYCLE_1_0) {
				dev_warn(sspi->dev, "sample mode %d over old delay cycle\n", sspi->sample_mode);
				sspi->sample_mode = SUNXI_SPI_SAMP_DELAY_CYCLE_0_0;
			}
			sspi->sample_delay = 0;
		}
	}

	if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL)
		dev_info(sspi->dev, "spi manual set sample mode_%d, delay_%d\n", sspi->sample_mode, sspi->sample_delay);
}

u32 sunxi_spi_calibrate_get_bus_sample_mode(struct udevice *dev)
{
	struct sunxi_spi *sspi = dev_get_priv(dev->parent);
	return sspi->bus_sample_mode;
}
EXPORT_SYMBOL_GPL(sunxi_spi_calibrate_get_bus_sample_mode);

u32 sunxi_spi_calibrate_get_sample_mode(struct udevice *dev)
{
	struct sunxi_spi *sspi = dev_get_priv(dev->parent);
	return sspi->sample_mode;
}
EXPORT_SYMBOL_GPL(sunxi_spi_calibrate_get_sample_mode);

u32 sunxi_spi_calibrate_get_sample_delay(struct udevice *dev)
{
	struct sunxi_spi *sspi = dev_get_priv(dev->parent);
	return sspi->sample_delay;
}
EXPORT_SYMBOL_GPL(sunxi_spi_calibrate_get_sample_delay);

int sunxi_spi_calibrate_set_sample_param(struct udevice *dev, u32 bus_sample_mode, u32 sample_mode, u32 sample_delay)
{
	struct sunxi_spi *sspi = dev_get_priv(dev->parent);

	if ((sspi->bus_mode & (SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND)) &&
		(sspi->data->quirk_flag & NEW_SAMPLE_MODE)) {
		if (sample_mode > SUNXI_SPI_SAMP_DELAY_CYCLE_3_0) {
			dev_err(sspi->dev, "sample mode %d over new delay cycle\n", sample_mode);
			return -EINVAL;
		}
		if (sample_delay > SUNXI_SPI_SAMPLE_DELAY_CHAIN_MAX) {
			dev_err(sspi->dev, "sample delay %d over new delay chain\n", sample_delay);
			return -EINVAL;
		}
	} else {
		if (sample_mode > SUNXI_SPI_SAMP_DELAY_CYCLE_1_0) {
			dev_err(sspi->dev, "sample mode %d over old delay cycle\n", sample_mode);
			return -EINVAL;
		}
		sample_delay = 0;
	}
	sspi->bus_sample_mode = bus_sample_mode;
	sspi->sample_mode = sample_mode;
	sspi->sample_delay = sample_delay;
	if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_MANUAL)
		sunxi_spi_delay_chain_init(sspi);
	else if (sspi->bus_sample_mode == SUNXI_SPI_SAMP_MODE_AUTO) {
		if ((sspi->bus_mode & (SUNXI_SPI_BUS_MASTER | SUNXI_SPI_BUS_NOR | SUNXI_SPI_BUS_NAND)) &&
			(sspi->data->quirk_flag & NEW_SAMPLE_MODE))
			sunxi_spi_set_sample_delay_sw(sspi, false);
		sunxi_spi_set_delay_chain(sspi, sspi->speed_hz);
	}
	return 0;
}
EXPORT_SYMBOL(sunxi_spi_calibrate_set_sample_param);
