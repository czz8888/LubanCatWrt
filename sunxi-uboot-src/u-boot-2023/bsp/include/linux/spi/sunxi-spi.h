/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright(c) 2020 - 2023 Allwinner Technology Co.,Ltd. All rights reserved. */
/*
 * SUNXI SPI Header Definition
 *
 */

#ifndef __LINUX_SUNXI_SPI_H
#define __LINUX_SUNXI_SPI_H

#include <spi.h>

enum sunxi_spi_sample_mode {
	SUNXI_SPI_SAMP_MODE_AUTO = 0,
	SUNXI_SPI_SAMP_MODE_MANUAL = 1,
};

enum sunxi_spi_sample_delay_cycle {
	SUNXI_SPI_SAMP_DELAY_CYCLE_0_0 = 0,
	SUNXI_SPI_SAMP_DELAY_CYCLE_0_5 = 1,
	SUNXI_SPI_SAMP_DELAY_CYCLE_1_0 = 2,
	SUNXI_SPI_SAMP_DELAY_CYCLE_1_5 = 3,
	SUNXI_SPI_SAMP_DELAY_CYCLE_2_0 = 4,
	SUNXI_SPI_SAMP_DELAY_CYCLE_2_5 = 5,
	SUNXI_SPI_SAMP_DELAY_CYCLE_3_0 = 6,
};

enum sunxi_spi_sample_delay_chain {
	SUNXI_SPI_SAMPLE_DELAY_CHAIN_MIN = 0,
	SUNXI_SPI_SAMPLE_DELAY_CHAIN_MAX = 63,
};

#define SUNXI_SPI_SAMP_MODE_DL_DEFAULT	(0xaaaaffff)

extern u32 sunxi_spi_calibrate_get_sample_mode(struct udevice *dev);
extern u32 sunxi_spi_calibrate_get_sample_delay(struct udevice *dev);
extern u32 sunxi_spi_calibrate_get_bus_sample_mode(struct udevice *dev);
extern int sunxi_spi_calibrate_set_sample_param(struct udevice *dev, u32 bus_sample_mode, u32 sample_mode, u32 sample_delay);

#endif	/* __LINUX_SUNXI_SPI_H */
