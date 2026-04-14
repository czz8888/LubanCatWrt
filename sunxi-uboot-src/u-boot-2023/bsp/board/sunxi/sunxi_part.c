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

#include <part.h>
#include <sunxi_board.h>
#include <sunxi_mbr.h>
#include <sunxi_gpt.h>
#include <sunxi_flash.h>
#include <u-boot/crc.h>

/* This function is not yet completed, wait for completion
 *	This thing was ported from u-boot-2018
 *	Mainly used to improve access efficiency
*/

int sunxi_probe_partition_map(void)
{
#ifndef CONFIG_ENABLE_MTD_CMDLINE_PARTS_BY_ENV
	struct blk_desc *desc;
	int ret = 0;
	//desc = blk_get_devnum_by_typename("sunxi_flash", 0);
	if (get_boot_storage_type() == STORAGE_EMMC) {
		ret = blk_get_device_by_str("mmc", "0", &desc);
		if (ret < 0)
			return -1;
	}
#if 0
	if (desc == NULL) {
		pr_err("%s: get desc fail\n", __func__);
		return -1;
	}
	if (part_init_info_map(desc) < 0)
	    return -1;
	else
#endif
#endif
	return 0;
}

int sunxi_mbr_convert_to_gpt(void *sunxi_mbr_buf, char *gpt_buf,
			     int storage_type)
{
	legacy_mbr *remain_mbr;
	sunxi_mbr_t *sunxi_mbr = (sunxi_mbr_t *)sunxi_mbr_buf;

	char *pbuf = gpt_buf;
	gpt_header *gpt_head;
	gpt_entry *pgpt_entry = NULL;
	char *gpt_entry_start = NULL;
	u32 data_len = 0;
	int total_sectors;
	u32 logic_offset = 0;
	int i, j = 0;

	unsigned char guid[16] = { 0x88, 0x38, 0x6f, 0xab, 0x9a, 0x56,
				   0x26, 0x49, 0x96, 0x68, 0x80, 0x94,
				   0x1d, 0xcb, 0x40, 0xbc };
	unsigned char part_guid[16] = { 0x46, 0x55, 0x08, 0xa0, 0x66, 0x41,
					0x4a, 0x74, 0xa3, 0x53, 0xfc, 0xa9,
					0x27, 0x2b, 0x8e, 0x45 };
	efi_guid_t basic_data_guid = PARTITION_BASIC_DATA_GUID;

	if (strncmp((const char *)sunxi_mbr->magic, SUNXI_MBR_MAGIC, 8)) {
		pr_err("%s:not sunxi mbr, can't convert to GPT partition\n",
		       __func__);
		return 0;
	}

	if (crc32(0, (const unsigned char *)(sunxi_mbr_buf + 4),
		  SUNXI_MBR_SIZE - 4) != sunxi_mbr->crc32) {
		pr_err("%s:sunxi mbr crc error, can't convert to GPT partition\n",
		       __func__);
		return 0;
	}

	sunxi_get_logical_offset_param(storage_type, &logic_offset,
				       &total_sectors);

	/* 1. LBA0: write legacy mbr,part type must be 0xee */
	remain_mbr = (legacy_mbr *)pbuf;
	memset(remain_mbr, 0x0, 512);
	remain_mbr->partition_record[0].sector = 0x2;
	remain_mbr->partition_record[0].cyl = 0x0;
	remain_mbr->partition_record[0].sys_ind = EFI_PMBR_OSTYPE_EFI_GPT;
	remain_mbr->partition_record[0].end_head = 0xFF;
	remain_mbr->partition_record[0].end_sector = 0xFF;
	remain_mbr->partition_record[0].end_cyl = 0xFF;
	remain_mbr->partition_record[0].start_sect = 1UL;
	remain_mbr->partition_record[0].nr_sects = 0xffffffff;
	remain_mbr->signature = MSDOS_MBR_SIGNATURE;
	data_len += 512;

	/* 2. LBA1: fill primary gpt header */
	gpt_head = (gpt_header *)(pbuf + data_len);
	gpt_head->signature = GPT_HEADER_SIGNATURE;
	gpt_head->revision = GPT_HEADER_REVISION_V1;
	gpt_head->header_size = sizeof(gpt_header);
	gpt_head->header_crc32 = 0x00;
	gpt_head->reserved1 = 0x0;
	gpt_head->my_lba = 0x01;
	gpt_head->alternate_lba = total_sectors - 1;
	gpt_head->first_usable_lba = sunxi_mbr->array[0].addrlo + logic_offset;
	if (storage_type == STORAGE_NOR) {
		/*spinor do not have much space, drop backup gpt to enlarge UDISK*/
		gpt_head->last_usable_lba = total_sectors - 1;
	} else {
		/*room for backup GPT consider "unsable":1 GPT head + 32 GPT entry*/
		gpt_head->last_usable_lba = total_sectors - (1 + 32) - 1;
	}
	memcpy(gpt_head->disk_guid.b, guid, 16);
	gpt_head->partition_entry_lba = 2;
	gpt_head->num_partition_entries = sunxi_mbr->PartCount;
	gpt_head->sizeof_partition_entry = GPT_ENTRY_SIZE;
	gpt_head->partition_entry_array_crc32 = 0;
	data_len += 512;

	/* 3. LBA2~LBAn: fill gpt entry */
	gpt_entry_start = (pbuf + data_len);
	for (i = 0; i < sunxi_mbr->PartCount; i++) {
		pgpt_entry =
			(gpt_entry *)(gpt_entry_start + (i)*GPT_ENTRY_SIZE);

		memcpy((void *)&(pgpt_entry->partition_type_guid),
		       (void *)&basic_data_guid, sizeof(basic_data_guid));

		memcpy(pgpt_entry->unique_partition_guid.b, part_guid, 16);
		pgpt_entry->unique_partition_guid.b[15] = part_guid[15] + i;

		pgpt_entry->starting_lba =
			((u64)sunxi_mbr->array[i].addrhi << 32) +
			sunxi_mbr->array[i].addrlo + logic_offset;
		pgpt_entry->ending_lba =
			pgpt_entry->starting_lba +
			((u64)sunxi_mbr->array[i].lenhi << 32) +
			sunxi_mbr->array[i].lenlo - 1;

		/* UDISK partition */
		if (i == sunxi_mbr->PartCount - 1) {
			pgpt_entry->ending_lba = gpt_head->last_usable_lba;
#if defined(CONFIG_AW_MTD_SPINAND) || defined(CONFIG_AW_MTD_RAWNAND)
			if ((STORAGE_SPI_NAND == get_boot_storage_type_ext()) ||
			     (STORAGE_NAND == get_boot_storage_type_ext())) {
				/* backup gpt not belong to any volumes */
				pgpt_entry->ending_lba =
					pgpt_entry->starting_lba +
					sunxi_mbr->array[i].lenlo - 1;
			}
#endif
		}

		pr_debug("GPT:%-12s: %-12llx  %-12llx\n",
			 sunxi_mbr->array[i].name, pgpt_entry->starting_lba,
			 pgpt_entry->ending_lba);

		if (sunxi_mbr->array[i].ro == 1) {
			pgpt_entry->attributes.fields.type_guid_specific =
				0x6000;
		} else {
			pgpt_entry->attributes.fields.type_guid_specific =
				0x8000;
		}

		if (sunxi_mbr->array[i].keydata == 0x8000) {
			pgpt_entry->attributes.fields.keydata = 1;
		}

		//ASCII to unicode
		memset(pgpt_entry->partition_name, 0,
		       PARTNAME_SZ * sizeof(efi_char16_t));
		if (!strncmp((char *)sunxi_mbr->array[i].name, "UDISK",
			     sizeof("UDISK"))) {
			char temp_partition_name[16] = {
				CONFIG_LAST_PARTITION_NAME
			};
			for (j = 0;
			     j < strlen((const char *)temp_partition_name);
			     j++) {
				pgpt_entry->partition_name[j] =
					(efi_char16_t)temp_partition_name[j];
			}
			/* update last partiton name ok set gpt_head->reserved1 = 0x1 */
			gpt_head->reserved1 = 0x1;
		} else {
			for (j = 0;
			     j < strlen((const char *)sunxi_mbr->array[i].name);
			     j++) {
				pgpt_entry->partition_name[j] =
					(efi_char16_t)sunxi_mbr->array[i]
						.name[j];
			}
		}
		data_len += GPT_ENTRY_SIZE;
	}

	//entry crc
	gpt_head->partition_entry_array_crc32 =
		crc32(0, (unsigned char const *)gpt_entry_start,
		      (gpt_head->num_partition_entries) *
			      (gpt_head->sizeof_partition_entry));

	pr_debug("gpt_head->partition_entry_array_crc32 = 0x%x\n",
		 gpt_head->partition_entry_array_crc32);
	//gpt crc
	gpt_head->header_crc32 =
		crc32(0, (const unsigned char *)gpt_head, sizeof(gpt_header));
	pr_debug("gpt_head->header_crc32 = 0x%x\n", gpt_head->header_crc32);

	/* 4. LBA-1: the last sector fill backup gpt header */

	return data_len;
}

int gpt_convert_to_sunxi_mbr(void *sunxi_mbr_buf, char *gpt_buf, int storage_type)
{
	u32 data_len	  = 0;
	char *pbuf	    = gpt_buf;
	gpt_entry *pgpt_entry = NULL;
	char *gpt_entry_start = NULL;
	int PartCount	 = 0;
	int pos		      = 0;
	int i, j;
	int crc32_total;
	int mbr_size = 0;
	u64 start_sector;
	int total_sectors;
	u32 logic_offset = 0;

	/* mbr_size = 256; [> hardcode, TODO: fixit <] */
	/* mbr_size = mbr_size * (1024/512); */

	sunxi_mbr_t *sunxi_mbr = (sunxi_mbr_t *)sunxi_mbr_buf;
	memset(sunxi_mbr, 0, sizeof(sunxi_mbr_t));

	sunxi_mbr->version = 0x00000200;
	memcpy(sunxi_mbr->magic, SUNXI_MBR_MAGIC, 8);
	sunxi_mbr->copy = 1;

	data_len = 0;
	data_len += 512; /* 0 to gpt->head */
	data_len += 512; /* gpt->head to gpt_entry */
	gpt_entry_start = (pbuf + data_len);
	while (1) {
		pgpt_entry = (gpt_entry *)(gpt_entry_start + (pos)*GPT_ENTRY_SIZE);
		if ((long)(pgpt_entry->starting_lba) > 0) {
			printf("gpt pos:%d s:0x%llx e:0x%llx\n", pos, pgpt_entry->starting_lba, pgpt_entry->ending_lba);
			PartCount++;
		} else
			break;

		pos++;
	}
	sunxi_mbr->PartCount = PartCount;
	printf("PartCount = %d\n", PartCount);
	sunxi_mbr->PartCount += 1; //need UDISK
	printf("fix, now PartCount = %d\n", sunxi_mbr->PartCount);

	for (i = sunxi_mbr->PartCount - 1; i >= 0; i--) {
		/* udisk is the first part */
		/* pos = (i == sunxi_mbr->PartCount-1) ? 0: i+1; */
		pos	= i;
		pgpt_entry = (gpt_entry *)(gpt_entry_start + (pos)*GPT_ENTRY_SIZE);

		sunxi_mbr->array[i].lenhi = ((pgpt_entry->ending_lba - pgpt_entry->starting_lba + 1) >> 32) & 0xffffffff;
		sunxi_mbr->array[i].lenlo = ((pgpt_entry->ending_lba - pgpt_entry->starting_lba + 1) >> 0) & 0xffffffff;
		if (i == sunxi_mbr->PartCount - 1) {
			sunxi_mbr->array[i].lenhi = 0;
			sunxi_mbr->array[i].lenlo = 0;
		};
		printf("%d starting_lba:%llx ending_lba:%llx\n", i, pgpt_entry->starting_lba, pgpt_entry->ending_lba);
		if (i == 0) {
			/* mbr_size = pgpt_entry->ending_lba - 511; */
			sunxi_get_logical_offset_param(storage_type, &logic_offset, &total_sectors);
			mbr_size = pgpt_entry->starting_lba - logic_offset;
			printf("mbr sector size:%x\n", mbr_size);
		}

		if (pgpt_entry->attributes.fields.type_guid_specific == 0x6000)
			sunxi_mbr->array[i].ro = 1;
		else if (pgpt_entry->attributes.fields.type_guid_specific == 0x8000)
			sunxi_mbr->array[i].ro = 0;

		strcpy((char *)sunxi_mbr->array[i].classname, "DISK");
		memset(sunxi_mbr->array[i].name, 0, 16);

		for (j = 0; j < 16; j++) {
			if (pgpt_entry->partition_name[j])
				sunxi_mbr->array[i].name[j] = pgpt_entry->partition_name[j];
			else
				break;
		}
	}

	for (i = 0; i < sunxi_mbr->PartCount; i++) {
		if (i == 0) {
			sunxi_mbr->array[i].addrhi = 0;
			sunxi_mbr->array[i].addrlo = mbr_size;
		} else {
			start_sector = sunxi_mbr->array[i - 1].addrlo;
			start_sector |= (u64)sunxi_mbr->array[i - 1].addrhi << 32;
			start_sector += sunxi_mbr->array[i - 1].lenlo;

			sunxi_mbr->array[i].addrlo = (u32)(start_sector & 0xffffffff);
			sunxi_mbr->array[i].addrhi = (u32)((start_sector >> 32) & 0xffffffff);
		}
		printf("i=%d, addrhi=%d addrlo=0x%x, lenhi=0x%x, lenlo=0x%x, name=%s\n", i, sunxi_mbr->array[i].addrhi,
		       sunxi_mbr->array[i].addrlo, sunxi_mbr->array[i].lenhi, sunxi_mbr->array[i].lenlo, sunxi_mbr->array[i].name);
	}

	crc32_total      = crc32(0, (const unsigned char *)(sunxi_mbr_buf + 4), SUNXI_MBR_SIZE - 4);
	sunxi_mbr->crc32 = crc32_total;

	return 0;
}

