// SPDX-License-Identifier:	GPL-2.0+
/*
 * (C) Copyright 2007-2013
 * Allwinner Technology Co., Ltd. <www.allwinnertech.com>
 * Jerry Wang <wangflord@allwinnertech.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <common.h>
#include <sunxi_board.h>
#include <private_toc.h>
#include <private_boot0.h>
#include <private_uboot.h>
#include <asm/arch/rtc.h>
#include <asm/global_data.h>
#include <sprite.h>
#include <sunxi_flash.h>
#include <memalign.h>
#include <sunxi_mbr.h>
#include <sunxi_gpt.h>
#include <u-boot/crc.h>
#include <part_efi.h>
#include <sunxi_part.h>
#include <sunxi_image_verifier.h>
#ifdef CONFIG_AW_MTD_SPINAND
#include <linux/mtd/aw-spinand.h>
#endif
#ifdef CONFIG_AW_MTD_RAWNAND
#include <linux/mtd/aw-rawnand.h>
#endif
#ifdef CONFIG_SUNXI_DUAL_STORAGE
#include <sunxi_dual_storage.h>
#endif

DECLARE_GLOBAL_DATA_PTR;

#define GPT_BUFF_4K_SIZE        (8 * 1024 * 8)
#define SUNXI_UFS_ALIGN_SECTOR 8
/*16(first boot0 address)+1400(boot0 max size 668k,align to 700k) sector=177 block(4K block size)**/
#define SUNXI_UFS_PARTITION_ENTRY_LAB_4K  ((16+ (700*1024)/512)/SUNXI_UFS_ALIGN_SECTOR)

int download_standard_gpt(void *sunxi_mbr_buf, size_t buf_size, int storage_type);
extern int sunxi_set_secure_mode(void);
extern int get_boot_storage_type(void);

void dump_dram_para(void *dram, uint size)
{
	int i;
	uint *addr = (uint *)dram;

	for (i = 0; i < size; i++) {
		printf("dram para[%d] = %x\n", i, addr[i]);
	}
}

static void prepare_backup_gpt_header(gpt_header *gpt_h)
{
	uint32_t calc_crc32;
	uint64_t val;

	/* recalculate the values for the Backup GPT Header */
	val = le64_to_cpu(gpt_h->my_lba);
	gpt_h->my_lba = gpt_h->alternate_lba; /*total_sectors - 1*/
	gpt_h->alternate_lba = cpu_to_le64(val);
	gpt_h->partition_entry_lba =
		cpu_to_le64(le64_to_cpu(gpt_h->last_usable_lba) + 1);
	gpt_h->header_crc32 = 0;

	calc_crc32 = crc32(0, (const unsigned char *)gpt_h,
			   le32_to_cpu(gpt_h->header_size));
	gpt_h->header_crc32 = cpu_to_le32(calc_crc32);
}

static int compare_image_and_flash_size(u8 *buf)
{
	sunxi_mbr_t *mbr_info = (sunxi_mbr_t *)buf;
	sunxi_partition *part_info;
	u32 i;
	char buffer[32];
	uint logic_size = 0;
	uint flash_size = 0;

	for (part_info = mbr_info->array, i = 0; i < mbr_info->PartCount;
	     i++, part_info++) {
		memset(buffer, 0, 32);
		memcpy(buffer, part_info->name, 16);
		//NOTE : Use UDISK addrlo as the size of image
		if ((!strcmp(buffer, "UDISK")) ||
		    (!strcmp(buffer, CONFIG_LAST_PARTITION_NAME))) {
			logic_size = part_info->addrlo;
		}
	}
	flash_size = sunxi_flash_size();
	pr_notice("logic size : 0x%x sector(512 byte)\n", logic_size);
	pr_notice("flash size : 0x%x sector(512 byte)\n", flash_size);

	if (flash_size == 0) {
		pr_notice("flash size is zero, ignore check\n");
		return 0;
	}
	if (flash_size > logic_size) {
		return 0;
	} else {
		pr_err("error : logic_size biger than flash_size\n");
		return -1;
	}
}

int sunxi_sprite_download_mbr(void *buffer, uint buffer_size)
{
	int ret = 0;
	int storage_type = get_boot_storage_type();
	int mbr_num = SUNXI_MBR_COPY_NUM;

	if (storage_type == STORAGE_NOR) {
		mbr_num = 1;
	}

	if (buffer_size != (SUNXI_MBR_SIZE * mbr_num)) {
		pr_err("the mbr size is bad\n");
		return -1;
	}

	if (compare_image_and_flash_size(buffer)) {
		pr_err("please check you image and flash size!\n");
		pr_err("=============Exit burning now=============\n");
		return -1;
	}

	// storage_type = get_boot_storage_type();
	// if ((sunxi_sprite_init(0)) && (storage_type == STORAGE_NAND)) {
	// 	return -2;
	// }
	/*write GPT Table*/
	ret = download_standard_gpt(buffer, buffer_size, storage_type);
	if (ret) {
		return -3;
	}
	pr_notice("update partition map\n");
	sunxi_probe_partition_map();

	return 0;
}

#ifdef CONFIG_SUNXI_PRIVATE_UBIDEV
int sunxi_sprite_download_private_ubidev(void *buffer, uint length, int production_media)
{
	printf("private size = 0x%x\n", length);
	printf("storage type = %d\n", production_media);

	return sunxi_sprite_download_priate(buffer, length);

}
#endif

int sunxi_sprite_download_uboot(void *buffer, int production_media,
				int generate_checksum)
{
	u32 length = 0;

	sbrom_toc1_head_info_t *toc1 = (sbrom_toc1_head_info_t *)buffer;

	if (toc1->magic != TOC_MAIN_INFO_MAGIC) {
		pr_notice("sunxi sprite: toc magic is error\n");
		pr_notice("need %s image\n",
			  gd->bootfile_mode == SUNXI_BOOT_FILE_TOC ? "secure" :
								     "normal");
		return -1;
	}
	length = toc1->valid_len;
	if (generate_checksum) {
		toc1->add_sum = sunxi_sprite_generate_checksum(
			buffer, toc1->valid_len, toc1->add_sum);
	}

	pr_notice("uboot size = 0x%x\n", length);
	pr_notice("storage type = %d\n", production_media);

#if defined(CONFIG_SUNXI_BURN_ROTPK_ON_SPRITE) ||                              \
	defined(CONFIG_SUNXI_ROTPK_BURN_ENABLE_BY_TOOL)
	sunxi_verify_preserve_toc1(buffer);
#endif

#ifdef CONFIG_SUNXI_DUAL_STORAGE
	sunxi_dual_storage_handle(SUNXI_DUAL_STORAGE_SWITCH, get_boot_work_mode());
#endif

	return sunxi_flash_download_toc(buffer, length, production_media);
}

#if defined(CONFIG_SUNXI_ROTPK_BURN_ENABLE_BY_TOOL)
int set_rotpk_flag(unsigned char *flag)
{
	if ((uboot_spare_head.boot_data.func_mask &
			UBOOT_FUNC_MASK_BIT_BURN_ROTPK) !=
			UBOOT_FUNC_MASK_BIT_BURN_ROTPK) {
		printf("tool did not set rotpk burn flag, skip rotpk burn\n");
		*flag = 0;
		return 0;
	}
	if (gd->securemode == SUNXI_NORMAL_MODE) {
		printf("normal mode, don't need set rotpk flag\n");
		*flag = 0;
		return 0;
	}

	printf("set rotpk flag to toc0 header\n");
	*flag = 1;

	return 0;
}
#endif

int download_secure_boot0(void *buffer, int production_media)
{
	toc0_private_head_t *toc0 = (toc0_private_head_t *)buffer;
	sbrom_toc0_config_t *toc0_config = NULL;
	int ret = 0;

	if (toc0->items_nr == 3)
		toc0_config = (sbrom_toc0_config_t *)(buffer + 0xa0);
	else
		toc0_config = (sbrom_toc0_config_t *)(buffer + 0x80);

	pr_notice("%s\n", (char *)toc0->name);
	if (strncmp((const char *)toc0->name, TOC0_MAGIC, MAGIC_SIZE)) {
		pr_notice(
			"sunxi sprite: toc0 magic is error, need secure image\n");

		return -1;
	}

	if (sunxi_sprite_verify_checksum(buffer, toc0->length,
					 toc0->check_sum)) {
		pr_notice("sunxi sprite: toc0 checksum is error\n");

		return -1;
	}

	//update flash param
	if (!production_media) {
#if (defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND))
	ubi_nand_get_flash_info((void *)toc0_config->storage_data, STORAGE_BUFFER_SIZE);
#else
		pr_notice("not define RAWNAND & SPINAND\n");
		return -1;
#endif
	}
#ifdef CONFIG_AW_SPINOR
	else if (production_media == STORAGE_NOR) {
			spinor_update_boot0_param((void *)toc0_config->storage_data);
	}
#endif
	else {
#if CONFIG_IS_ENABLED(AW_MMC)
		//storage_data[384];  // 0-159:nand info  160-255:card info
		if (production_media == STORAGE_EMMC) {
			if (mmc_write_info(
				    2,
				    (void *)(toc0_config->storage_data + 160),
				    384 - 160)) {
				pr_notice("add sdmmc2 gpio info fail!\n");
				return -1;
			}
		} else if (production_media == STORAGE_EMMC3) {
			if (mmc_write_info(
				    3,
				    (void *)(toc0_config->storage_data + 160),
				    384 - 160)) {
				pr_notice("add sdmmc3 gpio info fail!\n");
				return -1;
			}
		} else if (production_media == STORAGE_EMMC0) {
			if (mmc_write_info(
				    0,
				    (void *)(toc0_config->storage_data + 160),
				    384 - 160)) {
				pr_notice("add sdmmc0 gpio info fail!\n");
				return -1;
			}
		}
#endif
	}

#ifdef CONFIG_SUNXI_TURNNING_DRAM
	//update dram param
	if (uboot_spare_head.boot_data.work_mode == WORK_MODE_CARD_PRODUCT ||
	    (uboot_spare_head.boot_data.work_mode == WORK_MODE_BOOT &&
	     get_boot_dram_update_flag())) {
		memcpy((void *)toc0_config->dram_para,
		       (void *)(uboot_spare_head.boot_data.dram_para), 32 * 4);
		/*update dram flag*/
		set_boot_dram_update_flag(toc0_config->dram_para);
	} else if (uboot_spare_head.boot_data.work_mode ==
			   WORK_MODE_UDISK_UPDATE ||
		   uboot_spare_head.boot_data.work_mode ==
			   WORK_MODE_CARD_UPDATE ||
		   uboot_spare_head.boot_data.work_mode ==
			   WORK_MODE_SPRITE_RECOVERY ||
		   uboot_spare_head.boot_data.work_mode == WORK_MODE_BOOT) {
		pr_notice("skip memcpy dram para for work_mode: %d \n",
		       uboot_spare_head.boot_data.work_mode);
	} else {
		memcpy((void *)toc0_config->dram_para,
		       (void *)CONFIG_DRAM_PARA_ADDR, 32 * 4);
		/*update dram flag*/
		set_boot_dram_update_flag(toc0_config->dram_para);
	}
#endif
#ifndef CONFIG_SUNXI_BOOT_PARAM
	dump_dram_para(toc0_config->dram_para, 32);
#endif
#if defined(CONFIG_SUNXI_ROTPK_BURN_ENABLE_BY_TOOL)
	set_rotpk_flag(&toc0_config->rotpk_flag);
#endif

	/* regenerate check sum */
	toc0->check_sum = sunxi_sprite_generate_checksum(buffer, toc0->length,
							 toc0->check_sum);
	if (sunxi_sprite_verify_checksum(buffer, toc0->length,
					 toc0->check_sum)) {
		pr_notice("sunxi sprite: boot0 checksum is error\n");
		return -1;
	}
	pr_notice("storage type = %d\n", production_media);
	ret = sunxi_flash_download_spl(buffer, toc0->length, production_media);

	if (!ret) {
		pr_notice("burn sboot ok, set secure bit\n");
		ret = sunxi_set_secure_mode();
	}

	return ret;
}

int download_normal_boot0(void *buffer, int production_media)
{
	boot0_file_head_t *boot0 = (boot0_file_head_t *)buffer;

	if (strncmp((const char *)boot0->boot_head.magic, BOOT0_MAGIC,
		    MAGIC_SIZE)) {
		pr_notice("%8s\n", boot0->boot_head.magic);
		pr_notice("sunxi sprite: boot0 magic is error\n");
		return -1;
	}

	if (sunxi_sprite_verify_checksum(buffer, boot0->boot_head.length,
					 boot0->boot_head.check_sum)) {
		pr_err("sunxi sprite: boot0 checksum is error\n");
		return -1;
	}

	if (!production_media) {
#if (defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND))
		ubi_nand_get_flash_info((void *)boot0->prvt_head.storage_data, STORAGE_BUFFER_SIZE);
#else
		pr_notice("not define RAWNAND & SPINAND\n");
		return -1;
#endif
	}
#ifdef CONFIG_AW_SPINOR
	else if (production_media == STORAGE_NOR) {
			spinor_update_boot0_param((void *)boot0->prvt_head.storage_data);
	}
#endif
	else {
#if CONFIG_IS_ENABLED(AW_MMC)
		if (production_media == STORAGE_EMMC) {
			if (mmc_write_info(
				    2, (void *)boot0->prvt_head.storage_data,
				    STORAGE_BUFFER_SIZE)) {
				pr_notice("add sdmmc2 private info fail!\n");
				return -1;
			}
		} else if (production_media == STORAGE_EMMC3) {
			if (mmc_write_info(
				    3, (void *)boot0->prvt_head.storage_data,
				    STORAGE_BUFFER_SIZE)) {
				pr_notice("add sdmmc3 private info fail!\n");
				return -1;
			}
		} else if (production_media == STORAGE_EMMC0) {
			if (mmc_write_info(
				    0, (void *)boot0->prvt_head.storage_data,
				    STORAGE_BUFFER_SIZE)) {
				pr_notice("add sdmmc0 private info fail!\n");
				return -1;
			}
		}
#endif
	}


#ifdef CONFIG_SUNXI_TURNNING_DRAM
	if (uboot_spare_head.boot_data.work_mode == WORK_MODE_CARD_PRODUCT ||
	    (uboot_spare_head.boot_data.work_mode == WORK_MODE_BOOT &&
	     get_boot_dram_update_flag())) {
		memcpy((void *)&boot0->prvt_head.dram_para,
		       (void *)(uboot_spare_head.boot_data.dram_para), 32 * 4);
		/*update dram flag*/
		set_boot_dram_update_flag(boot0->prvt_head.dram_para);
	} else if (uboot_spare_head.boot_data.work_mode ==
			   WORK_MODE_UDISK_UPDATE ||
		   uboot_spare_head.boot_data.work_mode ==
			   WORK_MODE_CARD_UPDATE ||
		   uboot_spare_head.boot_data.work_mode ==
			   WORK_MODE_SPRITE_RECOVERY ||
		   uboot_spare_head.boot_data.work_mode == WORK_MODE_BOOT) {
		pr_notice("skip memcpy dram para for work_mode: %d \n",
		       uboot_spare_head.boot_data.work_mode);
	} else {
		memcpy((void *)boot0->prvt_head.dram_para,
		       (void *)CONFIG_DRAM_PARA_ADDR, 32 * 4);
		/*update dram flag*/
		set_boot_dram_update_flag(boot0->prvt_head.dram_para);
	}
#endif
#ifndef CONFIG_SUNXI_BOOT_PARAM
	dump_dram_para(boot0->prvt_head.dram_para, 32);
#endif


	/* regenerate check sum */
	boot0->boot_head.check_sum = sunxi_sprite_generate_checksum(
		buffer, boot0->boot_head.length, boot0->boot_head.check_sum);
	if (sunxi_sprite_verify_checksum(buffer, boot0->boot_head.length,
					 boot0->boot_head.check_sum)) {
		pr_notice("sunxi sprite: boot0 checksum is error\n");
		return -1;
	}
	pr_notice("storage type = %d\n", production_media);
	return sunxi_flash_download_spl(buffer, boot0->boot_head.length,
					production_media);
}

int sunxi_sprite_get_boot0_cheksum(void *buffer, int *boot0_checksum)
{
	if (gd->bootfile_mode == SUNXI_BOOT_FILE_NORMAL ||
	    gd->bootfile_mode == SUNXI_BOOT_FILE_PKG) {
		boot0_file_head_t *boot0 = (boot0_file_head_t *)buffer;
		if (strncmp((const char *)boot0->boot_head.magic, BOOT0_MAGIC,
			    MAGIC_SIZE)) {
			printf("sunxi sprite: boot0 magic is error\n");
			goto ERR_OUT;
		}
		*boot0_checksum = boot0->boot_head.check_sum;
		return 0;
	} else {
		toc0_private_head_t *toc0 = (toc0_private_head_t *)buffer;
		if (strncmp((const char *)toc0->name, TOC0_MAGIC, MAGIC_SIZE)) {
			printf("sunxi sprite: toc0 magic is error\n");
			goto ERR_OUT;
		}
		*boot0_checksum = toc0->check_sum;
		return 0;
	}

ERR_OUT:
	return -1;
}

int sunxi_sprite_download_boot0(void *buffer, int production_media)
{
#ifdef CONFIG_SUNXI_DUAL_STORAGE
	sunxi_dual_storage_handle(SUNXI_DUAL_STORAGE_SWITCH, get_boot_work_mode());
#endif

	if (gd->bootfile_mode == SUNXI_BOOT_FILE_NORMAL ||
	    gd->bootfile_mode == SUNXI_BOOT_FILE_PKG) {
		return download_normal_boot0(buffer, production_media);
	} else {
		return download_secure_boot0(buffer, production_media);
	}
	/* usb dma recv time out maybe enter usb product twice
     * brom enter fel mode or boot0 enter fel mode
     */
	rtc_set_bootmode_flag(0);
}

int download_standard_gpt(void *sunxi_mbr_buf, size_t buf_size,
			  int storage_type)
{
	typedef long (*FLASH_WIRTE)(lbaint_t start_block, lbaint_t nblock, void *buffer);
	FLASH_WIRTE flash_write_pt = NULL;
	char  *gpt_buf = NULL;
#ifdef CONFIG_SUNXI_UFS
	char  *gpt4k_buf = NULL;
	int  gpt4k_buf_len = 8*1024*8;
	int data4k_len = 0;
#endif
	int   data_len = 0;
	int   ret = 0;
	gpt_header   *gpt_head;
	int  gpt_buf_len = 8*1024;

#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
	if (((STORAGE_SPI_NAND == get_boot_storage_type_ext()) ||
			(STORAGE_NAND == get_boot_storage_type_ext()))) {
		pr_notice("force mbr\n");
		ret = sunxi_sprite_write(0, buf_size>>9, sunxi_mbr_buf);
		if (!ret) {
			pr_notice("%s:write mbr sectors fail ret = %d\n", __func__, ret);
			sunxi_sprite_write_end();
			sunxi_sprite_flush();
			return -1;
		}
		sunxi_sprite_write_end();
		sunxi_sprite_flush();
		return 0;
	}
#endif
	gpt_buf = memalign(CONFIG_SYS_CACHELINE_SIZE,
			   ALIGN(gpt_buf_len, CONFIG_SYS_CACHELINE_SIZE));
	if (gpt_buf == NULL) {
		pr_err("malloc for GPT fail\n");
		return -1;
	}
	memset(gpt_buf, 0x0, gpt_buf_len);

#ifdef CONFIG_SUNXI_UFS
	if (storage_type == STORAGE_UFS) {
		gpt4k_buf = memalign(CONFIG_SYS_CACHELINE_SIZE, ALIGN(gpt4k_buf_len, CONFIG_SYS_CACHELINE_SIZE));
		if (gpt4k_buf == NULL) {
			debug("malloc for GPT  fail\n");
			return -1;
		}
		memset(gpt4k_buf, 0x0, gpt4k_buf_len);
	}
#endif

	data_len = sunxi_mbr_convert_to_gpt(sunxi_mbr_buf, gpt_buf, storage_type);
	if (data_len == 0) {
		goto __err_end;
	}
	debug("gpt data len %d\n", data_len);

#ifdef CONFIG_SUNXI_UFS
	if (storage_type == STORAGE_UFS) {
		data4k_len = sunxi_mbr_convert_to_gpt_4k(sunxi_mbr_buf, gpt4k_buf, storage_type);
		if (data4k_len == 0) {
			goto __err_end;
		}
	}
#endif

	/*write GPT for u-boot use*/
	ret = sunxi_sprite_write(0, (data_len + 511) >> 9, gpt_buf);
	if (!ret) {
		pr_err("%s:write gpt sectors fail\n", __func__);
		goto __err_end;
	}
	/*write GPT for kenerl use if sdmmc*/
	if (STORAGE_EMMC == storage_type || STORAGE_EMMC3 == storage_type ||
	    storage_type == STORAGE_EMMC0) {
		ret = sunxi_sprite_phywrite(0, (data_len + 511) >> 9, gpt_buf);
		if (!ret)
			goto __err_end;
	} else if (storage_type == STORAGE_UFS) {
#ifdef CONFIG_SUNXI_UFS
		printf("ufs:write 4096 align mbr first\n");
		/*only write lba0,lba1,total 8k=16sector**/
		ret = sunxi_sprite_phywrite(0, 16, gpt4k_buf);
		if (!ret)
			goto __err_end;
		/*4096*2 skip gpt lba0,lba1**/
		/*write gpt entry to  SUNXI_UFS_PARTITION_ENTRY_LAB_4K**/
		ret = sunxi_sprite_phywrite(SUNXI_UFS_PARTITION_ENTRY_LAB_4K * SUNXI_UFS_ALIGN_SECTOR, (data4k_len - 4096*2 + 511) >> 9, gpt4k_buf + 4096*2);
		if (!ret)
			goto __err_end;
		printf("write primary GPT4k success\n");
#else
		printf("no ufs gpt\n");
#endif
	}
	pr_notice("write primary GPT success\n");

#ifndef CONFIG_SUNXI_UFS
	gpt_head = (gpt_header *)(gpt_buf + GPT_HEAD_OFFSET);
	prepare_backup_gpt_header(gpt_head);
#else
	if (storage_type == STORAGE_UFS) {
		gpt_head = (gpt_header *)(gpt4k_buf + GPT_HEAD_4K_OFFSET);
		prepare_backup_gpt_header(gpt_head);
	} else {
		gpt_head = (gpt_header *)(gpt_buf + GPT_HEAD_OFFSET);
		prepare_backup_gpt_header(gpt_head);
	}
#endif

	if (STORAGE_NOR == storage_type) {
		pr_notice("spinor: skip backup GPT\n");
	} else {
		if (STORAGE_EMMC == storage_type ||
		    STORAGE_EMMC3 == storage_type ||
		    storage_type == STORAGE_EMMC0 ||
			(storage_type == STORAGE_UFS))
			flash_write_pt = sunxi_sprite_phywrite;
		else
			flash_write_pt = sunxi_sprite_write;

#ifdef CONFIG_SUNXI_UFS
		if (storage_type == STORAGE_UFS) {
				/* write back-up gpt PTE */
			ret = flash_write_pt((gpt_head->last_usable_lba + 1) * SUNXI_UFS_ALIGN_SECTOR,
					     (GPT_BUFF_4K_SIZE - GPT_ENTRY_4K_OFFSET) / 512,
					     gpt4k_buf + GPT_ENTRY_4K_OFFSET);
			if (!ret) {
				goto __err_end;
			}

			/* write back-up gpt HEAD */
			ret = flash_write_pt((gpt_head->my_lba) * SUNXI_UFS_ALIGN_SECTOR, 1,
							gpt4k_buf + GPT_HEAD_4K_OFFSET);
			if (!ret) {
				goto __err_end;
			}

			printf("write 4k Backup GPT success\n");
		} else
#endif
		{
			/* write back-up gpt PTE */
			ret = flash_write_pt(gpt_head->last_usable_lba + 1,
							(GPT_BUFF_SIZE - GPT_ENTRY_OFFSET) / 512,
							gpt_buf + GPT_ENTRY_OFFSET);
			if (!ret) {
				goto __err_end;
			}

			/* write back-up gpt HEAD */
			ret = flash_write_pt(gpt_head->my_lba, 1,
							gpt_buf + GPT_HEAD_OFFSET);
			if (!ret) {
				goto __err_end;
			}
		}
		pr_notice("write Backup GPT success\n");
	}

#ifdef CONFIG_SUNXI_UFS
	if ((storage_type == STORAGE_UFS) && gpt4k_buf) {
		free(gpt4k_buf);
	}
#endif
	free(gpt_buf);

	return 0;
__err_end:
#ifdef CONFIG_SUNXI_UFS
	if ((storage_type == STORAGE_UFS) && gpt4k_buf) {
		free(gpt4k_buf);
	}
#endif

	if (gpt_buf)
		free(gpt_buf);
	return -1;
}

//TODO
/*
int sunxi_sprite_verify_checksum(void *buffer, uint length, uint src_sum)
{
	return sunxi_verify_checksum(buffer, length, src_sum);
}
*/



