// SPDX-License-Identifier: GPL-2.0+
/*
 * MMC driver for allwinner sunxi platform.
 *
 */
#include <common.h>
#include <command.h>
#include <errno.h>
#include <mmc.h>
#include <part.h>
#include <malloc.h>
#include <linux/list.h>
#include <div64.h>
#include <linux/math64.h>
#include <sunxi_flash.h>
#include <dm.h>
#include <privatestorage.h>
#include <spare_head.h>
#include <private_toc.h>
#include <private_boot0.h>
#include <sunxi_boot_param.h>
#include <sunxi_board.h>
#include <sprite_download.h>
#include <sprite_verify.h>
#include <sunxi_mbr.h>

//secure storage relate
#define MAX_SECURE_STORAGE_MAX_ITEM             32
#define SDMMC_SECURE_STORAGE_START_ADD  (6*1024*1024/512)//6M
#define SDMMC_ITEM_SIZE                                 (4*1024/512)//4K

#define SUNXI_SDMMC_PARAMETER_REGION_LBA_START 24504
#define MMC_BOOT_PARAM_COMMON_OFFSET 8
#define DEFAULT_MMC_BOOTPARAM_COMMOM_OFFSET                                    \
	(SUNXI_SDMMC_PARAMETER_REGION_LBA_START - MMC_BOOT_PARAM_COMMON_OFFSET)
#define DEFAULT_MMC_BOOTPARAM_COMMOM_SIZE (BOOT_PARAM_SIZE / 512)


/*transfer pointer to unsigned type,if phy add is 64,trasfer to u64*/
#define PT_TO_PHU(p)   ((unsigned long)(p))

extern int mmc_set_blocklen(struct mmc *mmc, int len);
extern int mmc_read_blocks(struct mmc *mmc, void *dst, lbaint_t start, lbaint_t blkcnt);
extern ulong mmc_write_blocks(struct mmc *mmc, lbaint_t start, lbaint_t blkcnt, const void *src);
extern ulong mmc_erase_t(struct mmc *mmc, ulong start, lbaint_t blkcnt);
extern int mmc_poll_for_busy(struct mmc *mmc, int timeout_ms);

int sunxi_flash_mmc_phyread(struct udevice *dev, lbaint_t start, lbaint_t blkcnt, void *buffer)
{
	lbaint_t cur, blocks_todo = blkcnt;
	uint b_max;
	struct sunxi_flash_desc *block_dev = dev_get_uclass_plat(dev);
	int dev_num = block_dev->devnum;

	if (blkcnt == 0)
		return 0;

	struct mmc *mmc = find_mmc_device(dev_num);
	if (!mmc)
		return 0;

	block_dev->lba = lldiv(mmc->capacity, mmc->read_bl_len);
	if ((start + blkcnt) > block_dev->lba) {
#if !defined(CONFIG_SPL_BUILD) || defined(CONFIG_SPL_LIBCOMMON_SUPPORT)
		pr_err("MMC: block number 0x" LBAF " exceeds max(0x" LBAF ")\n",
			start + blkcnt, block_dev->lba);
#endif
		return 0;
	}

	if (mmc_set_blocklen(mmc, mmc->read_bl_len)) {
		pr_debug("%s: Failed to set blocklen\n", __func__);
		return 0;
	}

	b_max = mmc_get_b_max(mmc, buffer, blkcnt);

	do {
		cur = (blocks_todo > b_max) ? b_max : blocks_todo;
		if (mmc_read_blocks(mmc, buffer, start, cur) != cur) {
			pr_debug("%s: Failed to read blocks\n", __func__);
			return 0;
		}
		blocks_todo -= cur;
		start += cur;
		buffer += cur * mmc->read_bl_len;
	} while (blocks_todo > 0);

	return blkcnt;
}

int sunxi_flash_mmc_phywrite(struct udevice *dev, lbaint_t start,
			    lbaint_t blkcnt, void *buffer)
{
#if CONFIG_IS_ENABLED(MMC_WRITE)
	lbaint_t cur, blocks_todo = blkcnt;
	struct sunxi_flash_desc *block_dev = dev_get_uclass_plat(dev);
	int dev_num = block_dev->devnum;

	struct mmc *mmc = find_mmc_device(dev_num);
	if (!mmc)
		return 0;

	if (mmc_set_blocklen(mmc, mmc->write_bl_len))
		return 0;

	do {
		cur = (blocks_todo > mmc->cfg->b_max) ?
			mmc->cfg->b_max : blocks_todo;
		if (mmc_write_blocks(mmc, start, cur, buffer) != cur)
			return 0;
		blocks_todo -= cur;
		start += cur;
		buffer += cur * mmc->write_bl_len;
	} while (blocks_todo > 0);

	return blkcnt;
#else
	pr_err("%s...%d:unsupport mmc write\n", __func__, __LINE__);
	return 0;
#endif

}

int sunxi_flash_mmc_erase(struct udevice *dev, lbaint_t start, lbaint_t blkcnt, void *skip)
{
#if CONFIG_IS_ENABLED(MMC_WRITE)
	struct sunxi_flash_desc *block_dev = dev_get_uclass_plat(dev);
	int dev_num = block_dev->devnum;
	int err = 0;
	u32 start_rem, blkcnt_rem;
	struct mmc *mmc = find_mmc_device(dev_num);
	lbaint_t blk = 0, blk_r = 0;
	int timeout_ms = 1000;

	if (!mmc)
		return -1;

	/*
	 * We want to see if the requested start or total block count are
	 * unaligned.  We discard the whole numbers and only care about the
	 * remainder.
	 */
	err = div_u64_rem(start, mmc->erase_grp_size, &start_rem);
	err = div_u64_rem(blkcnt, mmc->erase_grp_size, &blkcnt_rem);
	if (start_rem || blkcnt_rem)
		pr_info("\n\nCaution! Your devices Erase group is 0x%x\n"
		       "The erase range would be change to "
		       "0x" LBAF "~0x" LBAF "\n\n",
		       mmc->erase_grp_size, start & ~(mmc->erase_grp_size - 1),
		       ((start + blkcnt + mmc->erase_grp_size - 1)
		       & ~(mmc->erase_grp_size - 1)) - 1);

	while (blk < blkcnt) {
		if (IS_SD(mmc) && mmc->ssr.au) {
			blk_r = ((blkcnt - blk) > mmc->ssr.au) ?
				mmc->ssr.au : (blkcnt - blk);
		} else {
			blk_r = ((blkcnt - blk) > mmc->erase_grp_size) ?
				mmc->erase_grp_size : (blkcnt - blk);
		}
		err = mmc_erase_t(mmc, start + blk, blk_r);
		if (err)
			break;

		blk += blk_r;

		/* Waiting for the ready status */
		if (mmc_poll_for_busy(mmc, timeout_ms))
			return 0;
	}

	return blk;
#else
	pr_err("%s...%d:unsupport mmc write\n", __func__, __LINE__);
	return 0;
#endif
}

ulong sunxi_flash_mmc_read(struct udevice *dev, lbaint_t start, lbaint_t blkcnt,
			void *buffer)
{
	pr_debug("mmcboot read: start 0x%lx, sector 0x%lx\n", start, blkcnt);
	return sunxi_flash_mmc_phyread(dev, start + CONFIG_SUNXI_SDMMC_LOGICAL_OFFSET, blkcnt, buffer);
}

ulong sunxi_flash_mmc_write(struct udevice *dev, lbaint_t start,
			    lbaint_t blkcnt, void *buffer)
{
#if CONFIG_IS_ENABLED(MMC_WRITE)
	pr_debug("mmcboot write: start 0x%lx, sector 0x%lx\n", start, blkcnt);
	return sunxi_flash_mmc_phywrite(dev, start + CONFIG_SUNXI_SDMMC_LOGICAL_OFFSET, blkcnt, buffer);
#else
	pr_err("%s...%d:unsupport mmc write\n", __func__, __LINE__);
	return 0;
#endif
}

#if CONFIG_IS_ENABLED(SUNXI_PRIVATE_STORAGE)
static unsigned char _inner_buffer[4096 + 64]; /*align temp buffer*/

int sunxi_flash_mmc_secread(struct udevice *dev, int item, unsigned char *buf, unsigned int nblock)
{
	int ret = 0;
	if (buf == NULL) {
		pr_err("input buf is NULL\n");
		ret = -1;
		goto OUT;
	}

	if (item > MAX_SECURE_STORAGE_MAX_ITEM) {
		pr_err("item exceed %d\n", MAX_SECURE_STORAGE_MAX_ITEM);
		ret = -1;
		goto OUT;
	}

	if (nblock > SDMMC_ITEM_SIZE) {
		pr_err("block count exceed %d\n", SDMMC_ITEM_SIZE);
		ret = -1;
		goto OUT;
	}
	if (sunxi_flash_mmc_phyread(dev,
		SDMMC_SECURE_STORAGE_START_ADD + SDMMC_ITEM_SIZE * 2 * item,
		nblock, buf) != nblock) {
		pr_err("read first backup failed in fun %s line %d\n", __FUNCTION__, __LINE__);
		ret = -1;
		goto OUT;
	}
OUT:
	return ret;
}

int sunxi_flash_mmc_secread_backup(struct udevice *dev, int item, unsigned char *buf, unsigned int nblock)
{
	int ret = 0;
	if (buf == NULL) {
		pr_err("input buf is NULL\n");
		ret = -1;
		goto OUT;
	}

	if (item > MAX_SECURE_STORAGE_MAX_ITEM) {
		pr_err("item exceed %d\n", MAX_SECURE_STORAGE_MAX_ITEM);
		ret = -1;
		goto OUT;
	}

	if (nblock > SDMMC_ITEM_SIZE) {
		pr_err("block count exceed %d\n", SDMMC_ITEM_SIZE);
		ret = -1;
		goto OUT;
	}
	if (sunxi_flash_mmc_phyread(dev,
		SDMMC_SECURE_STORAGE_START_ADD + SDMMC_ITEM_SIZE * 2 * item  + SDMMC_ITEM_SIZE,
		nblock, buf) != nblock) {
		pr_err("read second backup failed in fun %s line %d\n", __FUNCTION__, __LINE__);
		ret = -1;
		goto OUT;
	}
OUT:
	return ret;
}

static int mmc_secure_storage_read_map(struct udevice *dev, int item, unsigned char *buf,
				       unsigned int len)
{
	unsigned char *align;
	unsigned int blkcnt;
	int ret;

	if (PT_TO_PHU(buf) % 32) {
		align = (unsigned char *)((PT_TO_PHU(_inner_buffer) + 0x20) &
					  (~0x1f));
		memset(align, 0, 4096);
	} else {
		align = buf;
	}

	blkcnt = (len + 511) / 512;

	pr_info("read item%d copy0\n", item);
	ret = sunxi_flash_mmc_secread(dev, item, align, blkcnt);

	if (!ret) {
		/*read ok*/
		ret = sunxi_private_storage_check_map(align);
		if (ret == 0) {
			pr_err("the secure storage item0 copy0 is good\n");
			goto ok; /*copy 0 pass*/
		} else {
			pr_err("the secure storage item0 copy0 %s is bad\n",
				(ret == 2 ? "magic" : "crc"));
		}
	}

	// read backup
	memset(align, 0x0, len);
	pr_debug("read item%d copy1\n", item);

	ret = sunxi_flash_mmc_secread(dev, item, align, blkcnt);

	if (!ret) {
		ret = sunxi_private_storage_check_map(align);
		if (ret == 0) {
			pr_debug("the secure storage item0 copy1 is good\n");
		} else {
			pr_err("the secure storage item0 copy1 %s is bad\n",
				(ret == 2 ? "magic" : "crc"));
			memset(align, 0x0, len);
		}
		goto ok;
	}
	pr_err("unknown error happen in item 0 read\n");
	return -1;

ok:
	if (PT_TO_PHU(buf) % 32)
		memcpy(buf, align, len);
	return 0;
}

static int mmc_secure_storage_read_key(struct udevice *dev, int item,
				       unsigned char *buf, unsigned int len)
{
	unsigned char *align;
	unsigned int blkcnt;
	int ret;

	if (PT_TO_PHU(buf) % 32) {
		align = (unsigned char *)((PT_TO_PHU(_inner_buffer) + 0x20) &
					  (~0x1f));
		memset(align, 0, 4096);
	} else {
		align = buf;
	}

	blkcnt = (len + 511) / 512;

	ret = sunxi_flash_mmc_secread(dev, item, align, blkcnt);

	if (!ret) {
		/*check copy 0 */
		if (!sunxi_private_storage_check_key(align)) {
			pr_debug("the secure storage item%d copy0 is good\n",
				 item);
			goto ok; /*copy 0 pass*/
		}
		pr_err("the secure storage item%d copy0 is bad\n", item);
	}

	// read backup
	memset(align, 0x0, len);
	pr_debug("read item%d copy1\n", item);
	ret = (sunxi_flash_mmc_secread_backup(dev, item, align, blkcnt) == blkcnt) ?
		      0 :
		      -1;

	if (!ret) {
		/*check copy 1 */
		if (!sunxi_private_storage_check_key(align)) {
			pr_debug("the secure storage item%d copy1 is good\n",
				 item);
			goto ok; /*copy 1 pass*/
		}
		pr_err("the secure storage item%d copy1 is bad\n", item);
	}

	pr_err("sunxi_secstorage_read fail\n");
	return -1;

ok:
	if (PT_TO_PHU(buf) % 32)
		memcpy(buf, align, len);
	return 0;
}

ulong sunxi_flash_mmc_private_storage_read(struct udevice *dev, int item,
					   void *buffer, unsigned int len)
{
	if (item == 0)
		return mmc_secure_storage_read_map(dev, item, buffer, len);
	else
		return mmc_secure_storage_read_key(dev, item, buffer, len);
}

int sunxi_flash_mmc_secwrite(struct udevice *dev, int item, unsigned char *buf, unsigned int nblock)
{
	int ret = 0;
	if (buf == NULL) {
		pr_err("input buf is NULL\n");
		ret = -1;
		goto OUT;
	}

	if (item > MAX_SECURE_STORAGE_MAX_ITEM) {
		pr_err("item exceed %d\n", MAX_SECURE_STORAGE_MAX_ITEM);
		ret = -1;
		goto OUT;
	}

	if (nblock > SDMMC_ITEM_SIZE) {
		pr_err("block count exceed %d\n", SDMMC_ITEM_SIZE);
		ret = -1;
		goto OUT;
	}

	if (sunxi_flash_mmc_phywrite(dev,
		SDMMC_SECURE_STORAGE_START_ADD + SDMMC_ITEM_SIZE * 2 * item,
		nblock, buf) != nblock) {
		pr_err("write first backup failed in fun %s line %d\n", __FUNCTION__, __LINE__);
		ret = -1;
		goto OUT;
	}

	if (sunxi_flash_mmc_phywrite(dev,
		SDMMC_SECURE_STORAGE_START_ADD + SDMMC_ITEM_SIZE * 2 * item + SDMMC_ITEM_SIZE,
		nblock, buf) != nblock) {
		pr_err("write second backup failed in fun %s line %d\n", __FUNCTION__, __LINE__);
		ret = -1;
		goto OUT;
	}
OUT:
	return ret;
}

ulong sunxi_flash_mmc_private_storage_write(struct udevice *dev, int item,
					    void *buffer, unsigned int len)
{
	unsigned char *align;
	unsigned int blkcnt;

	if (PT_TO_PHU(buffer) % 32) { // input buf not align
		align = (unsigned char *)((PT_TO_PHU(_inner_buffer) + 0x20) &
					  (~0x1f));
		memcpy(align, buffer, len);
	} else
		align = buffer;

	blkcnt = (len + 511) / 512;

	return sunxi_flash_mmc_secwrite(dev, item, align, blkcnt);
}
#endif

#if CONFIG_IS_ENABLED(SUNXI_SPRITE)
int card_verify_boot0(uint start_block, uint length)
{
	int ret      = 0;
	char *buffer = NULL;
	buffer       = (char *)memalign(CONFIG_SYS_CACHELINE_SIZE, ALIGN(length, CONFIG_SYS_CACHELINE_SIZE));
	if (!buffer) {
		printf("%s: malloc %d byte memory fail\n", __func__, length);
		return -1;
	}
	memset(buffer, 0, ALIGN(length, CONFIG_SYS_CACHELINE_SIZE));

	ret = sunxi_sprite_phyread(start_block, length / 512, buffer);
	if (!ret) {
		printf("%s: write boot0 from %d fail\n", __func__, start_block);
		goto ERR_OUT;
	}

	if (gd->bootfile_mode == SUNXI_BOOT_FILE_NORMAL ||
	    gd->bootfile_mode == SUNXI_BOOT_FILE_PKG) {
		boot0_file_head_t *boot0 = (boot0_file_head_t *)buffer;
		if (strncmp((const char *)boot0->boot_head.magic, BOOT0_MAGIC,
			    MAGIC_SIZE)) {
			debug("%8s\n", boot0->boot_head.magic);
			printf("sunxi sprite: boot0 magic is error\n");
			goto ERR_OUT;
		}
		if (sunxi_sprite_verify_checksum(buffer,
						 boot0->boot_head.length,
						 boot0->boot_head.check_sum)) {
			printf("sunxi sprite: boot0 checksum is error flash_check_sum=0x%x\n",
			       boot0->boot_head.check_sum);
			goto ERR_OUT;
		}
		ret = 1;
	} else {
		toc0_private_head_t *toc0 = (toc0_private_head_t *)buffer;
		debug("%s\n", (char *)toc0->name);
		if (strncmp((const char *)toc0->name, TOC0_MAGIC, MAGIC_SIZE)) {
			printf("sunxi sprite: toc0 magic is error\n");
			goto ERR_OUT;
		}
		if (sunxi_sprite_verify_checksum(buffer, toc0->length,
						 toc0->check_sum)) {
			printf("sunxi sprite: toc0 checksum is error flash_check_sum=0x%x\n",
			       toc0->check_sum);
			goto ERR_OUT;
		}
		ret = 1;
	}

ERR_OUT:
	if (buffer != NULL)
		free(buffer);

	if (!ret)
		return -1;
	else
		return 0;
}

static int sunxi_flash_mmc_download_spl(struct udevice *dev, unsigned char *buf, int len, unsigned int ext)
{
	uint32_t i, fail_count;
	uint32_t write_offset[2] = { CONFIG_SUNXI_SDMMC_BOOT0_BACKUP_START_ADDR, CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR };

#ifdef CONFIG_SUNXI_BOOT_PARAM
	typedef_sunxi_boot_param *sunxi_boot_param = gd->sunxi_boot_param_addr;
	int boot0_checksum			   = 0;
	int ret					   = -1;

	if (!sunxi_bootparam_format(sunxi_boot_param)) {
		ret = sunxi_sprite_get_boot0_cheksum(buf, &boot0_checksum);
		if (ret < 0) {
			return -1;
		}
		sunxi_bootparam_set_boot0_checksum(sunxi_boot_param,
						   boot0_checksum);
		sunxi_boot_param->header.check_sum = sunxi_generate_checksum(
		sunxi_boot_param, sizeof(typedef_sunxi_boot_param), 1,
		sunxi_boot_param->header.check_sum);
		if (sunxi_flash_mmc_phywrite(dev,
							DEFAULT_MMC_BOOTPARAM_COMMOM_OFFSET,
							DEFAULT_MMC_BOOTPARAM_COMMOM_SIZE, sunxi_boot_param) != DEFAULT_MMC_BOOTPARAM_COMMOM_SIZE) {
			pr_err("%s: write boot_param failed\n", __func__);
			return -1;
		}
	}
#endif

	fail_count = 0;
	for (i = 0; i < 2; i++) {
		if (sunxi_flash_mmc_phywrite(dev,
						     write_offset[i], len / 512,
						     buf) != (len / 512)) {
			pr_err("%s: write %s failed\n", __func__,
				 write_offset[i] ==
						 CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR ?
					 "main spl" :
					 "back spl");
			fail_count++;
		} else {
			pr_info("%s: write %s done\n", __func__,
				 write_offset[i] ==
						 CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR ?
					 "main spl" :
					 "back spl");
		}
		if (card_verify_boot0(write_offset[i], len) < 0) {
			return -1;
		}
	}
	if (fail_count == 2) {
		return -1;
	}

	return 0;
}

static int sunxi_flash_mmc_download_boot_param(struct udevice *dev)
{
	typedef_sunxi_boot_param *sunxi_boot_param = gd->sunxi_boot_param_addr;

	sunxi_boot_param->header.check_sum = sunxi_generate_checksum(
		sunxi_boot_param, sizeof(typedef_sunxi_boot_param), 1,
		sunxi_boot_param->header.check_sum);
	if (sunxi_flash_mmc_phywrite(dev,
					       DEFAULT_MMC_BOOTPARAM_COMMOM_OFFSET,
					       DEFAULT_MMC_BOOTPARAM_COMMOM_SIZE, sunxi_boot_param) != DEFAULT_MMC_BOOTPARAM_COMMOM_SIZE) {
		pr_err("%s: write boot_param failed\n", __func__);
		return -1;
	}
	return 0;
}

static int sunxi_flash_mmc_download_toc(struct udevice *dev, unsigned char *buf, int len, unsigned int ext)
{
	if (sunxi_flash_mmc_phywrite(dev, CONFIG_SUNXI_SDMMC_UBOOT_START_ADDR,
					       len / 512, buf) != (len / 512)) {
		pr_err("%s: write main uboot failed\n", __func__);
		return -1;
	}

	if (sunxi_flash_mmc_phywrite(dev, CONFIG_SUNXI_SDMMC_UBOOT_BACKUP_START_ADDR, len / 512, buf) != (len / 512)) {
		pr_err("%s: write back uboot failed\n", __func__);
		return -1;
	}

	return 0;
}
#endif

#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT) || \
CONFIG_IS_ENABLED(MMC_HS200_SUPPORT) || \
CONFIG_IS_ENABLED(MMC_HS400_SUPPORT)
extern int mmc_deinit(struct mmc *mmc);
#endif

static int sunxi_flash_mmc_exit(struct udevice *dev, int force)
{
	struct sunxi_flash_desc *block_dev = dev_get_uclass_plat(dev);
	int dev_num = block_dev->devnum;

	struct mmc *mmc = find_mmc_device(dev_num);
	if (!mmc)
		return -1;
#if CONFIG_IS_ENABLED(MMC_UHS_SUPPORT) || \
CONFIG_IS_ENABLED(MMC_HS200_SUPPORT) || \
CONFIG_IS_ENABLED(MMC_HS400_SUPPORT)
	return mmc_deinit(mmc);
#endif
	return 0;
}

static int sunxi_flash_mmc_flush(struct udevice *dev)
{
	return 0;
}

static uint sunxi_flash_mmc_size(struct udevice *dev)
{
	struct sunxi_flash_desc *block_dev = dev_get_uclass_plat(dev);
	int dev_num = block_dev->devnum;

	struct mmc *mmc = find_mmc_device(dev_num);
	/* add init judgment to slove get size stuck after gerrit Change 255027 */
	if (!mmc || !mmc->has_init)
		return 0;

	block_dev->lba = lldiv(mmc->capacity, mmc->read_bl_len);

	return block_dev->lba;
}

static uint sunxi_flash_mmc_get_logical_offset(struct udevice *dev)
{
	return CONFIG_SUNXI_SDMMC_LOGICAL_OFFSET;
}

static uint sunxi_flash_mmc_get_storage(struct udevice *dev)
{
	return STORAGE_EMMC;
}

static int mmc_mirror_boo0_copy(struct udevice *dev, uint32_t src, uint32_t dst)
{
	int ret, size;
	u8 buffer[512];
	u8 *verify_buffer = NULL;
	u8 *boot_buffer	  = NULL;
	uint32_t check_sum;
	int i;
	const int write_retry_cnt = 3;
	ret			  = sunxi_flash_mmc_phyread(dev, src, 1, buffer);
	if (ret != 1) {
		pr_err("sunxi_flash_mmc_phyread fail\n");
		return -1;
	}

	if (sunxi_get_secureboard()) {
		toc0_private_head_t *toc0 = NULL;
		toc0			  = (toc0_private_head_t *)buffer;
		if (strncmp((const char *)toc0->name, TOC0_MAGIC, MAGIC_SIZE)) {
			pr_err("src toc0 magic is bad\n");
			return -1;
		}
		check_sum = toc0->check_sum;
		size	  = toc0->length;
	} else {
		boot0_file_head_t *boot0;
		boot0 = (boot0_file_head_t *)buffer;
		if (strncmp((const char *)boot0->boot_head.magic, BOOT0_MAGIC,
			    MAGIC_SIZE)) {
			pr_err("src boot0 magic is bad\n");
			return -1;
		}
		check_sum = boot0->boot_head.check_sum;
		size	  = boot0->boot_head.length;
	}

	boot_buffer = (u8 *)malloc(size);
	if (!boot_buffer) {
		pr_err("malloc buf fail\n");
		return -1;
	}
	verify_buffer = (u8 *)malloc(size);
	if (!verify_buffer) {
		pr_err("malloc buf fail\n");
		goto MIRROR_FAILED_;
	}

	ret = sunxi_flash_mmc_phyread(dev, src, size / 512, boot_buffer);
	if (ret != (size / 512)) {
		pr_err("sunxi_flash_mmc_phyread fail\n");
		goto MIRROR_FAILED_;
	}

	if (sunxi_verify_checksum(boot_buffer, size, check_sum)) {
		pr_err("boot0 checksum is error\n");
		goto MIRROR_FAILED_;
	}

	for (i = 0; i < write_retry_cnt; i++) {
		ret = sunxi_flash_mmc_phywrite(dev, dst, size / 512, boot_buffer);
		if (ret != (size / 512)) {
			pr_err("sunxi_flash_mmc_phywrite backup fail\n");
			continue;
		}

		memset(verify_buffer, 0, size);
		ret = sunxi_flash_mmc_phyread(dev, dst, size / 512, verify_buffer);
		if (ret != (size / 512)) {
			pr_err("sunxi_flash_mmc_phyread backup fail\n");
			continue;
		}

		if (sunxi_verify_checksum(verify_buffer, size, check_sum)) {
			pr_err("recovery boot0 checksum is error\n");
			continue;
		}
	}
	if (boot_buffer)
		free(boot_buffer);
	if (verify_buffer)
		free(verify_buffer);
	return 0;

MIRROR_FAILED_:
	if (boot_buffer)
		free(boot_buffer);
	if (verify_buffer)
		free(verify_buffer);
	return -1;
}

static int sunxi_flash_mmc_recover_boot0_copy0(struct udevice *dev)
{
	/* check if boot from copy1, if so, copy0 is curruptted. */
	if (sunxi_get_active_boot0_id() != 0) {
		int ret;
		/* copy copy1 to copy0 to recovery it */
		ret = mmc_mirror_boo0_copy(dev,
			CONFIG_SUNXI_SDMMC_BOOT0_BACKUP_START_ADDR,
			CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR);
		if (0 == ret) {
			printf("recover boot0 copy0 ok\n");
		} else {
			printf("recover boot0 copy0 failed\n");
		}
		return ret;
	}
	pr_info("boot0 copy0 ok, do not need recover");
	return 0;
}

static int sunxi_flash_mmc_update_backup_toc0(struct udevice *dev)
{
	int ret;
	u8 buffer[512];
	toc0_private_head_t *toc0 = NULL;

	if (sunxi_get_securemode() == SUNXI_NORMAL_MODE) {
		pr_info("non secure, do not need to update backup toc0 from offline secure boot0\n");
		return 0;
	}

	/*check if backup already a toc0*/
	memset(buffer, 0x0, 512);
	ret = sunxi_flash_mmc_phyread(dev, CONFIG_SUNXI_SDMMC_BOOT0_BACKUP_START_ADDR, 1, buffer);
	if (ret != 1) {
		pr_err("sunxi_flash_mmc_phyread backup fail\n");
		return -1;
	}

	toc0 = (toc0_private_head_t *)buffer;
	if (strncmp((const char *)toc0->name, TOC0_MAGIC, MAGIC_SIZE) == 0) {
		pr_info("toc0 magic is ok\n");
		return 0;
	}

	pr_info("update emmc backup toc0 start\n");
	if (0 ==
	    mmc_mirror_boo0_copy(dev, CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR,
				 CONFIG_SUNXI_SDMMC_BOOT0_BACKUP_START_ADDR)) {
		printf("update emmc backup toc0 ok\n");
		return 0;
	} else {
		printf("update emmc backup toc0 failed\n");
		return -1;
	}

}

#if CONFIG_IS_ENABLED(SUNXI_SPRITE)
int card_erase_boot0(uint length, void *buffer, uint storage_type)
{
	int ret		   = 0;
	char *erase_buffer = NULL;

	erase_buffer = (char *)memalign(CONFIG_SYS_CACHELINE_SIZE, ALIGN(length, CONFIG_SYS_CACHELINE_SIZE));
	if (!erase_buffer) {
		debug("%s: malloc %d byte memory fail\n", __func__, length);
		return -1;
	}
	memset(erase_buffer, 0, length);

	//for card2
	if ((storage_type == STORAGE_EMMC) || (storage_type == STORAGE_EMMC0)) {
		printf("card2 erase boot0 \n");
		//write boot0 bankup copy firstly
		ret = sunxi_sprite_phywrite(CONFIG_SUNXI_SDMMC_BOOT0_BACKUP_START_ADDR,
					   length / 512, buffer);
		if (!ret) {
			debug("%s: write boot0 from %d fail\n", __func__,
			       CONFIG_SUNXI_SDMMC_BOOT0_BACKUP_START_ADDR);
			goto ERR_OUT;
		}
		ret = sunxi_sprite_phywrite(CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR, length / 512,
					   buffer);
		if (!ret) {
			debug("%s: write boot0 from %d fail\n", __func__,
			       CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR);
			goto ERR_OUT;
		}
#ifdef PLATFORM_SUPPORT_EMMC3
		ret = sunxi_sprite_phywrite(BOOT0_EMMC3_START_ADDR, length / 512,
					   erase_buffer);
		if (!ret) {
			debug("%s: write boot0 from %d fail\n", __func__,
			       BOOT0_EMMC3_START_ADDR);
			goto ERR_OUT;
		}
		ret = sunxi_sprite_phywrite(BOOT0_EMMC3_BACKUP_START_ADDR,
					   length / 512, erase_buffer);
		if (!ret) {
			debug("%s: write boot0 from %d fail\n", __func__,
			       BOOT0_EMMC3_BACKUP_START_ADDR);
			goto ERR_OUT;
		}
#endif
	} else {
//for card3
#ifdef PLATFORM_SUPPORT_EMMC3
		printf("card3 erase boot0 \n");
		//write boot0 bankup copy firstly
		ret = sunxi_sprite_phywrite(BOOT0_EMMC3_BACKUP_START_ADDR,
					   length / 512, buffer);
		if (!ret) {
			debug("%s: write boot0 from %d fail\n", __func__,
			       BOOT0_EMMC3_BACKUP_START_ADDR);
			goto ERR_OUT;
		}
		ret = sunxi_sprite_phywrite(BOOT0_EMMC3_START_ADDR, length / 512,
					   buffer);
		if (!ret) {
			debug("%s: write boot0 from %d fail\n", __func__,
			       BOOT0_EMMC3_START_ADDR);
			goto ERR_OUT;
		}

		ret = sunxi_sprite_phywrite(CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR, length / 512,
					   erase_buffer);
		if (!ret) {
			debug("%s: write boot0 from %d fail\n", __func__,
			       CONFIG_SUNXI_SDMMC_BOOT0_START_ADDR);
			goto ERR_OUT;
		}
		ret = sunxi_sprite_phywrite(CONFIG_SUNXI_SDMMC_BOOT0_BACKUP_START_ADDR,
					   length / 512, erase_buffer);
		if (!ret) {
			debug("%s: write boot0 from %d fail\n", __func__,
			       CONFIG_SUNXI_SDMMC_BOOT0_BACKUP_START_ADDR);
			goto ERR_OUT;
		}
#endif
	}

ERR_OUT:
	if (erase_buffer != NULL)
		free(erase_buffer);

	if (!ret)
		return -1;
	else
		return 0;
}

#define CARD_ERASE_BLOCK_BYTES (8 * 1024 * 1024)
#define CARD_ERASE_BLOCK_SECTORS (CARD_ERASE_BLOCK_BYTES / 512)

int card_erase(int erase, void *mbr_buffer)
{
	char *erase_buffer;
	sunxi_mbr_t *mbr = (sunxi_mbr_t *)mbr_buffer;
	unsigned int erase_head_sectors;
	unsigned int erase_head_addr;
	unsigned int erase_tail_sectors;
	unsigned int erase_tail_addr;
	unsigned int skip_space[1 + 2 * 2] = { 0 };
	unsigned int from, nr;
	int k, ret = 0;
	int i;

	printf("erase all part start\n");
	if (!erase) {
		return 0;
	}
	erase_buffer = (char *)memalign(CONFIG_SYS_CACHELINE_SIZE, ALIGN(CARD_ERASE_BLOCK_BYTES, CONFIG_SYS_CACHELINE_SIZE));
	if (!erase_buffer) {
		debug("card erase fail: unable to malloc memory for card erase\n");

		return -1;
	}
	memset(erase_buffer, 0, ALIGN(CARD_ERASE_BLOCK_BYTES, CONFIG_SYS_CACHELINE_SIZE));

	//erase boot0,write 0x00
	if (card_erase_boot0(32 * 1024, erase_buffer, get_boot_storage_type())) {
		return -1;
	}

	for (i = 1; i < mbr->PartCount; i++) {
		printf("erase %s part\n", mbr->array[i].name);
		// part > 16M
		if (mbr->array[i].lenlo > CARD_ERASE_BLOCK_SECTORS * 2) {
			erase_head_sectors = CARD_ERASE_BLOCK_SECTORS;
			erase_head_addr    = mbr->array[i].addrlo;
			//erase_tail_sectors = CARD_ERASE_BLOCK_SECTORS;
			erase_tail_sectors = 2 * 1024 * 1024 / 512;
			erase_tail_addr    = mbr->array[i].addrlo +
					  mbr->array[i].lenlo -
					  CARD_ERASE_BLOCK_SECTORS;
			// 8M < part <= 16M
		} else if (mbr->array[i].lenlo > CARD_ERASE_BLOCK_SECTORS) {
			erase_head_sectors = CARD_ERASE_BLOCK_SECTORS;
			erase_head_addr    = mbr->array[i].addrlo;
			//erase_tail_sectors = mbr->array[i].lenlo - CARD_ERASE_BLOCK_SECTORS;
			erase_tail_sectors = 2 * 1024 * 1024 / 512;
			erase_tail_addr    = mbr->array[i].addrlo +
					  mbr->array[i].lenlo -
					  erase_tail_sectors;
			// 0 < part <= 8M
		} else if (mbr->array[i].lenlo > 0) {
			erase_head_sectors = mbr->array[i].lenlo;
			erase_head_addr    = mbr->array[i].addrlo;
			erase_tail_sectors = 0;
			erase_tail_addr    = mbr->array[i].addrlo;
		} else {
			//printf("don't deal prat's length is 0 (%s) \n", mbr->array[i].name);
			//break;
			erase_head_sectors = CARD_ERASE_BLOCK_SECTORS;
			erase_head_addr    = mbr->array[i].addrlo;
			erase_tail_sectors = 0;
			erase_tail_addr    = mbr->array[i].addrlo;
		}

		from = mbr->array[i].addrlo + 40960;
		nr   = mbr->array[i].lenlo;
		ret  = sunxi_sprite_phyerase(from, nr, skip_space);
		if (ret == 0) {
			//printf("erase part from sector 0x%x to 0x%x ok\n", from, (from+nr-1));
		} else if (ret == 1) {
			for (k = 0; k < 2; k++) {
				if (skip_space[0] & (1 << k)) {
					debug("write zeros-%d: from 0x%x to 0x%x\n",
					       k, skip_space[2 * k + 1],
					       (skip_space[2 * k + 1] +
						skip_space[2 * k + 2] - 1));
					from = skip_space[2 * k + 1];
					nr   = skip_space[2 * k + 2];
					if (!sunxi_sprite_phywrite(
						    from, nr, erase_buffer)) {
						debug("card erase fail in erasing part %s\n",
						       mbr->array[i].name);
						free(erase_buffer);
						return -1;
					}
				}
			}
		} else if (ret == -1) {
			// erase head for partition
			if (!sunxi_sprite_write(erase_head_addr,
					       erase_head_sectors,
					       erase_buffer)) {
				debug("card erase fail in erasing part %s\n",
				       mbr->array[i].name);
				free(erase_buffer);
				return -1;
			}
			debug("erase prat's head from sector 0x%x to 0x%x\n",
			       erase_head_addr,
			       erase_head_addr + erase_head_sectors);

			// erase tail for partition
			if (erase_tail_sectors) {
				if (!sunxi_sprite_write(erase_tail_addr,
						       erase_tail_sectors,
						       erase_buffer)) {
					debug("card erase fail in erasing part %s\n",
					       mbr->array[i].name);
					free(erase_buffer);
					return -1;
				}
				debug("erase part's tail from sector 0x%x to 0x%x\n",
				       erase_tail_addr,
				       erase_tail_addr + erase_tail_sectors);
			}
		}
	}
	printf("card erase all\n");
	free(erase_buffer);

	//while((*(volatile unsigned int *)0) != 1);
	//printf("erase all part end\n");
	return 0;
}
#endif

#if CONFIG_IS_ENABLED(SUNXI_SPRITE)
static int sunxi_sprite_mmc_erase(struct udevice *dev, int erase, void *mbr_buffer)
{
	return card_erase(erase, mbr_buffer);
}
#endif

int sunxi_sprite_mmc_force_erase(struct udevice *dev)
{
	unsigned int skip_space[1 + 2 * 2] = { 0 };

	return sunxi_sprite_phyerase(0, sunxi_flash_mmc_size(dev), skip_space);
}

static const struct sunxi_flash_ops mmc_sunxi_flash_ops = {
	.exit = sunxi_flash_mmc_exit,
	.read = sunxi_flash_mmc_read,
	.write = sunxi_flash_mmc_write,
	.erase = NULL,
#if CONFIG_IS_ENABLED(SUNXI_SPRITE)
	.sprite_erase = sunxi_sprite_mmc_erase,
#else
	.sprite_erase = NULL,
#endif
	.force_erase = sunxi_sprite_mmc_force_erase,
	.flush = sunxi_flash_mmc_flush,
	.size = sunxi_flash_mmc_size,
	.get_logical_offset = sunxi_flash_mmc_get_logical_offset,
	.get_storage = sunxi_flash_mmc_get_storage,
	.phyread = sunxi_flash_mmc_phyread,
	.phywrite = sunxi_flash_mmc_phywrite,
	.phyerase = sunxi_flash_mmc_erase,
#if CONFIG_IS_ENABLED(SUNXI_SPRITE)
	.download_spl = sunxi_flash_mmc_download_spl,
	.download_boot_param = sunxi_flash_mmc_download_boot_param,
	.download_toc = sunxi_flash_mmc_download_toc,
#endif
	.upload_toc = NULL,
	.write_end = NULL,
	.erase_area = NULL,
	.recover_boot0_copy0 = sunxi_flash_mmc_recover_boot0_copy0,
	.update_backup_toc0 = sunxi_flash_mmc_update_backup_toc0,
#if CONFIG_IS_ENABLED(SUNXI_PRIVATE_STORAGE)
	.pristorage_read = sunxi_flash_mmc_private_storage_read,
	.pristorage_write = sunxi_flash_mmc_private_storage_write,
	.secstorage_flush = NULL,
	.secstorage_fast_write = NULL,
#endif
};

U_BOOT_DRIVER(mmc_sunxi_flash) = {
	.name		= "mmc_sunxi_flash",
	.id		= UCLASS_SUNXI_FLASH,
	.ops		= &mmc_sunxi_flash_ops,
	.flags		= DM_FLAG_OS_PREPARE,
};

int mmc_init_sunxi_flash_ops(struct udevice *dev)
{
	int ret = 0;
	struct udevice *bdev;
	ret = sunxi_flash_create_devicef(dev, "mmc_sunxi_flash", "sunxi_flash",
					UCLASS_MMC, dev_seq(dev), &bdev);
	if (ret) {
		pr_debug("Cannot create sunxi_flash device\n");
		return ret;
	}

	return 0;
}

