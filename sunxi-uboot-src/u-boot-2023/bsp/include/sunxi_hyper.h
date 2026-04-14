/*
 * (C) Copyright 2023-2025
 * allwinner Technology Co., Ltd. <www.allwinnertech.com>
 *
 * some simple description for this code
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 */
#include <linux/ctype.h>
#include <linux/types.h>
#include <common.h>
#include <inttypes.h>
#include <stdio_dev.h>
#include <asm/global_data.h>
#include <fdt_support.h>
#include <exports.h>
#include <fdtdec.h>
#include <sunxi_board.h>
#include <android_image.h>
#include <sys_partition.h>
#include <elf.h>
#include <elf_helpers.h>

#ifndef _SUNXI_HYPER_H
#define _SUNXI_HYPER_H


#define   HYPER_IMG_SIZE         (0x1000)
#define   BOOT_IMG_SIZE          (0x10000)


#define   HYPER_NODE             "/chosen/hyper"
#define   DOM1_KERNEL_NODE       "/chosen/dom1/module@0"
#define   DOM1_RAMDISK_NODE      "/chosen/dom1/module@1"
#define   DOM2_KERNEL_NODE       "/chosen/dom2/module@0"
#define   DOM2_RAMDISK_NODE      "/chosen/dom2/module@1"

#define   HYPER_ZONE_NAME        "hyper-img"
#define   DOM2_ZONE_NAME         "dom2-boot"


static u64 hyper_entry;


enum os_type {
	OS_TYPE_LINUX = 0,
	OS_TYPE_FREERTOS,
	OS_TYPE_INVALID,
};

static int get_parent_node_cells(void *fdt, int node, u32 *addr, u32 *size)
{
	int pnode;

	pnode = fdt_parent_offset(fdt, node);
	if (pnode < 0) {
		printf("Warning: can`t find parent node \n");
		return 0;
	}

	*addr = fdt_getprop_u32_default_node(fdt, pnode, 0, "#address-cells", 0);
	if (!addr) {
		printf("No address-cells\n");
		return 0;
	}

	*size = fdt_getprop_u32_default_node(fdt, pnode, 0, "#size-cells", 0);
	if (!size) {
		printf("No size-cells\n");
		return 0;
	}
	return pnode;
}


int kylo_dtb_update(void *fdt, char *name, u64 addr, u64 size)
{
	int nodeoffset;
	const fdt32_t *reg;
	fdt32_t cells[3];
	int len;
	char full_path[64];
	u32 address_cells = 0, size_cells = 0;
	int ret;

	nodeoffset = fdt_path_offset(fdt, name);
	if (nodeoffset < 0) {
		printf("Warning: can`t find %s node \n", name);
		return -1;
	}
	reg = fdt_getprop(fdt, nodeoffset, "reg", &len);
	if (!reg) {
		printf("Warning: module has no address.\n");
		return -1;
	}

	ret = get_parent_node_cells(fdt, nodeoffset, &address_cells, &size_cells);
	if (!ret || address_cells != 2 || size_cells != 1) {
		fdt_get_path(fdt, ret, full_path, sizeof(full_path));
		printf("#address-cells != 2, #size-cells != 1 in %s\n", full_path);
		return -1;
	}

	cells[0] = fdt32_to_cpu(addr >> 32);
	cells[1] = fdt32_to_cpu(addr);
	cells[2] = fdt32_to_cpu(size);
	fdt_setprop(fdt, nodeoffset, "reg", cells, sizeof(cells));

	return 0;
}


static u64 kylo_get_image_base(void *fdt, char *name)
{
	int node;

	node = fdt_path_offset(fdt, name);
	if (node < 0) {
		pr_info("Warning: can`t find %s node \n", name);
		return 0;
	}

	return fdt_get_base_address(fdt, node);
}

static inline void  kylo_set_hyper_entry(u64 entry)
{
	hyper_entry = entry;
}

u64 kylo_hyper_entry(void)
{
	return hyper_entry;
}

int kylo_load_hyper_img(void)
{
	uint hypervisor_offset = 0, hypervisor_len = 0;
	u64 hyper_base = 0;

	kylo_set_hyper_entry(0);

	hyper_base = kylo_get_image_base(working_fdt, HYPER_NODE);
	if (!hyper_base) {
		pr_err("can`t not get hyper image base in %s\n", HYPER_NODE);
		return -1;
	}

	sunxi_partition_get_info_byname(HYPER_ZONE_NAME, &hypervisor_offset, &hypervisor_len);
	if (!hypervisor_offset) {
		pr_err("can`t find part named %s\n", HYPER_ZONE_NAME);
		return -1;
	}

	sunxi_flash_read(hypervisor_offset, hypervisor_len, (void *)(ulong)hyper_base);

	kylo_set_hyper_entry(hyper_base);

	return 0;
}

/* Todo: only load linux boot.img */
static int kylo_load_linux_img(char *zone_name, int os_index)
{
	int ret = 0;
	ulong linux_img_base = 0;
	uint boot_offset = 0, boot_size = 0;
	ulong ramdisk_addr = 0, ramdisk_size = 0;
	ulong kernel_addr = 0, kernel_len = 0 ;

	char *node_name = (char *)malloc(32);
	if (node_name == NULL) {
		pr_err("could not malloc memory \n");
		return -1;
	}

	/* kernel node_name = "/chosen/dom{os_index}/module@0" */
	sprintf(node_name, "/chosen/dom%d/module@0", os_index);
	linux_img_base = kylo_get_image_base(working_fdt, node_name);
	if (!linux_img_base) {
		pr_err("can`t not get kernel image base in %s\n", node_name);
		return -1;
	}

	sunxi_partition_get_info_byname(zone_name, &boot_offset, &boot_size);
	if (!boot_offset) {
		pr_err("%s Can`t find name %s \n", __func__, zone_name);
		return -1;
	}

	sunxi_flash_read(boot_offset, boot_size, (void *)linux_img_base);

	ret = android_image_get_kernel((const struct andr_img_hdr *)linux_img_base, 0, &kernel_addr, &kernel_len);
	if (ret) {
		pr_err("can't found kernel in boot.img \n");
		return -1;
	}

	ret = android_image_get_ramdisk((const struct andr_img_hdr *)linux_img_base, &ramdisk_addr, &ramdisk_size);
	if (ret) {
		pr_err("can't found ramdisk in boot.img\n");
		return -1;
	}

	/* update kernel addr and size for dom2 */
	kylo_dtb_update(working_fdt, node_name, kernel_addr, kernel_len);

	/* ramisk node_name = "/chosen/dom{os_index}/module@1" */
	sprintf(node_name, "/chosen/dom%d/module@1", os_index);
	kylo_dtb_update(working_fdt, node_name, ramdisk_addr, ramdisk_size);

	free(node_name);

	return 0;
}

#define ROUND_DOWN(a, b)	((a) & ~((b)-1))
#define ROUND_UP(a, b)		(((a) + (b)-1) & ~((b)-1))

#define ROUND_DOWN_CACHE(a) ROUND_DOWN(a, CONFIG_SYS_CACHELINE_SIZE)
#define ROUND_UP_CACHE(a)   ROUND_UP(a, CONFIG_SYS_CACHELINE_SIZE)

static ulong kylo_load_freertos_elf(ulong elf_fw_addr)
{
	void *ehdr;
	void *phdr;
	void *dst = NULL;
	void *src = NULL;
	int i = 0;
	ulong ret = 0;
	u8 class = fw_elf_get_class(elf_fw_addr);
	uint32_t flush_start_addr = 0, flush_cache_size = 0;

	ehdr = (void *)elf_fw_addr;
	phdr = (void *)elf_fw_addr + elf_Ehdr_get_e_phoff(class, ehdr);

	if (!elf_Ehdr_get_e_phnum(class, ehdr)) {
		pr_err("there is no program header, please check whether the ELF file is correct\n");
		return 0;
	}

	/* Load each program header */
	for (i = 0; i < elf_Ehdr_get_e_phnum(class, ehdr); ++i) {

		//remap addresses
		dst = (void *)(ulong)elf_Phdr_get_p_paddr(class, phdr);


		src = (void *)elf_fw_addr + elf_Phdr_get_p_offset(class, phdr);
		pr_err("Loading phdr %i from 0x%p to 0x%p (%lli bytes)\n",
		      i, src, dst, elf_Phdr_get_p_filesz(class, phdr));

		if (elf_Phdr_get_p_filesz(class, phdr)) {
			ret += elf_Phdr_get_p_filesz(class, phdr);
			memcpy(dst, src, elf_Phdr_get_p_filesz(class, phdr));
		}

		/*
		 * In order to speed up firmware loading, don't clear .bss section in here.
		 * Remote processor will clean it when boot up.
		 */
		/*
		if (elf_Phdr_get_p_filesz(class, phdr) != elf_Phdr_get_p_memsz(class, phdr))
			memset(dst + elf_Phdr_get_p_filesz(class, phdr), 0x00,
			elf_Phdr_get_p_memsz(class, phdr) - elf_Phdr_get_p_filesz(class, phdr));
		*/

		/*
		 * In order to avoid current cpu write back the cache content to memory after
		 * remote processor boot up(it will cause strange issue), we need to flush cache.
		 * And the range of flush cache is all memory which is used by remote processor.
		 */
		flush_start_addr = ROUND_DOWN_CACHE((unsigned long)dst);
		flush_cache_size = ROUND_UP_CACHE(elf_Phdr_get_p_memsz(class, phdr));

		//pr_err("flush_start_addr: 0x%08x, flush_cache_size: 0x%08x\n", flush_start_addr, flush_cache_size);
		flush_cache(flush_start_addr, flush_cache_size);
		phdr += elf_size_of_Phdr(class);

	}

	return ret;
}

static int kylo_load_freertos_img(char *zone_name, int os_index)
{
	ulong rtos_img_base = 0;
	uint img_offset = 0, img_size = 0;
	u8 class_type;
	ulong rtos_entry;

	char *node_name = (char *)malloc(32);
	if (node_name == NULL) {
		pr_err("could not malloc memory \n");
		return -1;
	}

	sprintf(node_name, "/chosen/dom%d/module@0", os_index);
	rtos_img_base = kylo_get_image_base(working_fdt, node_name);
	if (!rtos_img_base) {
		pr_err("Can`t get rtos imge base in %s \n", node_name);
		return -1;
	}

	sunxi_partition_get_info_byname(zone_name, &img_offset, &img_size);
	if (!img_offset) {
		pr_err("Can`t find img name %s \n", zone_name);
		return -1;
	}

	sunxi_flash_read(img_offset, img_size, (void *)rtos_img_base);

	img_size = kylo_load_freertos_elf(rtos_img_base);

	class_type = fw_elf_get_class((u64)rtos_img_base);
	rtos_entry = elf_Ehdr_get_e_entry(class_type, (void *)rtos_img_base);

	kylo_dtb_update(working_fdt, node_name, rtos_entry, img_size);

	free(node_name);

	return 0;
}

static int kylo_get_os_num(void *fdt)
{
	int node;
	int ret = 0;

	node = fdt_path_offset(fdt, "/chosen");
	if (node < 0) {
		pr_err("Can`t find /chosen node \n");
		goto err;
	}

	ret = fdt_getprop_u32_default_node(fdt, node, 0, "kylo-os-num", -1);
	if (ret < 0) {
		pr_err("Can`t find kylo-os-num in /chosen \n");
		goto err;
	} else if (ret == 0) {
		pr_err("kylo-os-num must greater than 1 \n");
		goto err;
	}

	return ret;
err:
	return 0;

}

static int kylo_get_os_type(int os_index, char *name)
{
	int node;

	node = fdt_path_offset(working_fdt, name);
	if (node < 0) {
		pr_err("%s Can`t find %s node \n", __func__, name);
		return OS_TYPE_INVALID;
	}

	return fdt_getprop_u32_default_node(working_fdt, node, 0, "kylo-os-type", 0);
}

int kylo_load_os_img(void)
{
	int i, len, node;
	int os_num = 0;
	int os_type = OS_TYPE_INVALID;

	char *os_zone_name = NULL;

	char *node_name = (char *)malloc(32);
	if (node_name == NULL) {
		printf("could not malloc memory \n");
		return -1;
	}

	os_num = kylo_get_os_num(working_fdt);
	for (i = 1; i <= os_num; i++) {
		if (i == 1)			/* dom1 skip load image to memory */
			continue;

		sprintf(node_name, "/chosen/dom%d", i);

		node = fdt_path_offset(working_fdt, node_name);
		if (node < 0) {
			pr_err("%s Can`t find %s node \n", __func__, node_name);
			return node;
		}

		os_zone_name = (char *)fdt_getprop(working_fdt, node, "kylo-os-zone-name", &len);
		if (os_zone_name == NULL) {
			pr_err("Can`t find kylo-os-zone-name in %s \n", node_name);
			return -1;
		}

		os_type = kylo_get_os_type(i, node_name);
		switch (os_type) {
		case OS_TYPE_LINUX:
			kylo_load_linux_img(os_zone_name, i);
			break;
		case OS_TYPE_FREERTOS:
			kylo_load_freertos_img(os_zone_name, i);
			break;
		default:
			printf("invalid os type %d \n", os_type);
			break;
		}
	}

	free(node_name);

	return 0;
}

#endif /* __SUNXI_HYPER_H */
