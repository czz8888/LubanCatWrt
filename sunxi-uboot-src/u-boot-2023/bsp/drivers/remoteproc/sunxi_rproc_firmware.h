/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * drivers/remoteproc/sunxi_rproc_firmware.h
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

#ifndef __SUNXI_RPROC_FIRM_H
#define __SUNXI_RPROC_FIRM_H

struct vaddr_range_t {
	unsigned long vstart;
	unsigned long vend;
	unsigned long pstart;
};

#if IS_ENABLED(CONFIG_MACH_SUN55IW6)
#define ADDR_TPYE	ulong
#define RISCV_STATUS_STR	"allwinner,sun55iw6-riscv"
#define RISCV_GPIO_INT_STR	"allwinner,sun55iw6-riscv-gpio-int"
#define RISCV_UART_STR		"allwinner,sun55iw6-riscv-uart"
#define RISCV_SHARE_SPACE	"allwinner,sun55iw6-riscv-share-space"
#elif IS_ENABLED(CONFIG_MACH_SUN8IW22)
#define ADDR_TPYE	ulong
#define RISCV_STATUS_STR	"allwinner,sun8iw22-riscv"
#define RISCV_GPIO_INT_STR	"allwinner,sun8iw22-riscv-gpio-int"
#define RISCV_UART_STR		"allwinner,sun8iw22-riscv-uart"
#define RISCV_SHARE_SPACE	"allwinner,sun8iw22-riscv-share-space"
#endif

int get_elf_fw_entry(ulong elf_fw_addr);

int riscv_find_rsc_table(const void *fw_addr, size_t fw_size,
			 const void **table_ptr, size_t *table_sz);
const char *get_elf_fw_version(ulong elf_fw_addr, struct vaddr_range_t *addr_map, int map_size);
int show_img_version(const char *head_addr, u32 riscv_id);
int load_elf_fw(ulong elf_fw_addr, struct vaddr_range_t *addr_map, int map_size);
#endif
