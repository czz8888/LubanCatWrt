/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_share_gpio_irq.h
 *
 * Copyright (c) 2007-2025 Allwinnertech Co., Ltd.
 * Author: wujiayi <wujiayi@allwinnertech.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details
 *
 */

#ifndef __SHARE_GPIO_IRQ_H
#define __SHARE_GPIO_IRQ_H

#define DTS_CLOSE (0)
#define DTS_OPEN  (1)

struct dts_uart_pin_msg_t {
	unsigned int port;
	unsigned int port_num;
	unsigned int mul_sel;
};

struct dts_uart_msg_t {
	unsigned int status;
	unsigned int uart_port;
	struct dts_uart_pin_msg_t uart_pin_msg[2];
};

struct dts_gpio_int_t {
	unsigned int gpio_a;
	unsigned int gpio_b;
	unsigned int gpio_c;
	unsigned int gpio_d;
	unsigned int gpio_e;
	unsigned int gpio_f;
	unsigned int gpio_g;
};

struct dts_sharespace_t {
	/* sharespace status */
	unsigned int status;
	/* riscv write space msg */
	unsigned int riscv_write_addr;
	unsigned int riscv_write_size;
	/* arm write space msg */
	unsigned int arm_write_addr;
	unsigned int arm_write_size;
	/* riscv log space msg */
	unsigned int riscv_log_addr;
	unsigned int riscv_log_size;
};

/* dts msg about riscv */
struct dts_msg_t {
	/* riscv status */
	unsigned int riscv_status;
	/* uart */
	struct dts_uart_msg_t uart_msg;
	/* gpio int */
	struct dts_gpio_int_t gpio_int;
	/* shacespace */
	struct dts_sharespace_t dts_sharespace;
};

#if defined(CONFIG_MACH_SUN55IW6)
#define ADDR_TPYE	ulong
#define RISCV_STATUS_STR	"allwinner,sun55iw6-riscv"
#define RISCV_GPIO_INT_STR	"allwinner,sun55iw6-riscv-gpio-int"
#define RISCV_UART_STR		"allwinner,sun55iw6-riscv-uart"
#define RISCV_SHARE_SPACE	"allwinner,sun55iw6-riscv-share-space"
#elif defined(CONFIG_MACH_SUN8IW22)
#define ADDR_TPYE	ulong
#define RISCV_STATUS_STR	"allwinner,sun8iw22-riscv"
#define RISCV_GPIO_INT_STR	"allwinner,sun8iw22-riscv-gpio-int"
#define RISCV_UART_STR		"allwinner,sun8iw22-riscv-uart"
#define RISCV_SHARE_SPACE	"allwinner,sun8iw22-riscv-share-space"
#endif

#endif
