// SPDX-License-Identifier: GPL-2.0+
/*
 * SPINOR driver for allwinner sunxi platform.
 *
 */

#include <common.h>
#include <command.h>
#include <errno.h>
#include <dm.h>
#include <log.h>
#include <sunxi_flash.h>
#include <spi.h>
#include <spi_flash.h>
#include <blk.h>
#include <spare_head.h>
#include <private_boot0.h>
#include <asm/global_data.h>
#include <memalign.h>
#include <sunxi_board.h>
#include <boot_param.h>
#include <sprite_download.h>
#include <linux/sizes.h>
#include <linux/mtd/spi-nor.h>
#include "../../spi/spif-sunxi.h"
#include "../../spi/spi-sunxi.h"
#include "sf_internal.h"

DECLARE_GLOBAL_DATA_PTR;

#ifdef CONFIG_SPINOR_UBOOT_OFFSET
#define DEFAULT_SPINOR_UBOOT_OFFSET CONFIG_SPINOR_UBOOT_OFFSET
#else
#define DEFAULT_SPINOR_UBOOT_OFFSET (128)
#endif

#ifdef CONFIG_SPINOR_LOGICAL_OFFSET
#define DEFAULT_SPINOR_LOGICAL_OFFSET CONFIG_SPINOR_LOGICAL_OFFSET
#else
#define DEFAULT_SPINOR_LOGICAL_OFFSET (2016)
#endif

#define CONFIG_SPINOR_PARAM_SPACE_SIZE 8
#define CONFIG_SPINOR_SECURE_STORAGE_SIZE 64


void dump_spinor_info(boot_spinor_info_t *spinor_info)
{
	pr_debug("-----------------------\n"
		"magic:%s\n"
		"readcmd:0x%x\n"
		"read_mode:%d\n"
		"write_mode:%d\n"
		"flash_size:%dM\n"
		"addr4b_opcodes:%d\n"
		"erase_size:%d\n"
		"frequency:%d\n"
		"sample_mode:0x%x\n"
		"sample_delay:0x%x\n"
		"bootparam_addr:%d\n"
		"----------------------\n",
		spinor_info->magic, spinor_info->readcmd,
		spinor_info->read_mode, spinor_info->write_mode,
		spinor_info->flash_size, spinor_info->addr4b_opcodes,
		spinor_info->erase_size, spinor_info->frequency,
		spinor_info->sample_mode, spinor_info->sample_delay,
		spinor_info->bootparam_addr);
}

/**
 * Write a block of data to SPI flash, first checking if it is different from
 * what is already there.
 *
 * If the data being written is the same, then *skipped is incremented by len.
 *
 * @param flash		flash context pointer
 * @param offset	flash offset to write
 * @param len		number of bytes to write
 * @param buf		buffer to write from
 * @param cmp_buf	read buffer to use to compare data
 * @param skipped	Count of skipped data (incremented by this function)
 * @return NULL if OK, else a string containing the stage which failed
 */
static const char *_spi_flash_update_block(struct udevice *dev, struct spi_flash *flash,
				u32 offset, size_t len, const char *buf, char *cmp_buf, size_t *skipped)
{
	char *ptr = (char *)buf;
	uint i = 0;

	pr_debug("offset=%x sector, nor_sector_size=%d bytes, len=%d bytes\n",
	      offset/flash->sector_size, flash->sector_size, len);
	/* Read the entire sector so to allow for rewriting */
	if (log_ret(sf_get_ops(dev->parent)->read(dev->parent, offset, flash->sector_size, cmp_buf)))
		return "read";

	while (*(cmp_buf + i) == 0xff) {
		i++;
		if (i == flash->sector_size)
			goto already_erase;
	}

	/* Compare only what is meaningful (len) */
	if (memcmp(cmp_buf, buf, len) == 0) {
		pr_debug("Skip region %x size %zx: no change\n",
		      offset, len);
		*skipped += len;
		return NULL;
	}

	/* Erase the entire sector */
	if (log_ret(sf_get_ops(dev->parent)->erase(dev->parent, offset, flash->sector_size)))
		return "erase";

already_erase:
	/* If it's a partial sector, copy the data into the temp-buffer */
	if (len != flash->sector_size) {
		memcpy(cmp_buf, buf, len);
		ptr = cmp_buf;
	}
	/* Write one complete sector */
	if (log_ret(sf_get_ops(dev->parent)->write(dev->parent, offset, flash->sector_size, ptr)))
		return "write";

	return NULL;
}

/**
 * Update an area of SPI flash by erasing and writing any blocks which need
 * to change. Existing blocks with the correct data are left unchanged.
 *
 * @param flash		flash context pointer
 * @param offset	flash offset to write
 * @param len		number of bytes to write
 * @param buf		buffer to write from
 * @return 0 if ok, 1 on error
 */
static int _spi_flash_update(struct udevice *dev, struct spi_flash *flash,
								u32 offset, size_t len, const char *buf)
{
	const char *err_oper = NULL;
	char *cmp_buf;
	const char *end = buf + len;
	size_t todo;		/* number of bytes to do in this pass */
	size_t skipped = 0;	/* statistics */

	cmp_buf = memalign(ARCH_DMA_MINALIGN, flash->sector_size);
	if (cmp_buf) {
		for (; buf < end && !err_oper; buf += todo, offset += todo) {
			todo = min_t(size_t, end - buf, flash->sector_size);
			err_oper = _spi_flash_update_block(dev, flash, offset, todo,
					buf, cmp_buf, &skipped);
		}
	} else {
		err_oper = "malloc";
	}
	free(cmp_buf);

	if (err_oper) {
		printf("SPI flash failed in %s step\n", err_oper);
		return 1;
	}
	return 0;
}

static unsigned long sunxi_flash_spinor_read(struct udevice *dev, lbaint_t start,
			      lbaint_t blkcnt, void *buffer)
{
	u32 offset = (start + DEFAULT_SPINOR_LOGICAL_OFFSET) << 9;
	size_t len = blkcnt << 9;
	void *buf = buffer;
	if (blkcnt == 0)
		return 0;

	if (log_ret(sf_get_ops(dev->parent)->read(dev->parent, offset, len, buf)))
		return 0;
	else
		return blkcnt;
}

static __maybe_unused unsigned long _sunxi_flash_spinor_read(struct udevice *dev, lbaint_t start,
			      lbaint_t blkcnt, void *buffer)
{
	u32 offset = start << 9;
	size_t len = blkcnt << 9;
	void *buf = buffer;
	if (blkcnt == 0)
		return 0;

	if (log_ret(sf_get_ops(dev->parent)->read(dev->parent, offset, len, buf)))
		return 0;
	else
		return blkcnt;
}

static unsigned long _sunxi_flash_spinor_write(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt, void *buffer)
{
	struct spi_flash *flash = dev_get_uclass_priv(dev->parent);
	u32 erase_size = 0, i = 0;
	u32 erase_align_addr = 0;
	u32 erase_align_ofs = 0;
	u32 erase_align_size = 0;
	char *align_buf = NULL;
	int ret = 0;

	if (!flash)
		return 0;

	u32 offset = start << 9;
	size_t len = blkcnt << 9;
	void *buf = buffer;
	if (blkcnt == 0)
		return 0;

	erase_size = flash->erase_size;
	if (offset % erase_size) {
		printf("SF: write offset not multiple of erase size\n");
		align_buf = memalign(ARCH_DMA_MINALIGN, erase_size);
		if (!align_buf) {
			printf("%s: malloc error\n", __func__);
			return 0;
		}
		erase_align_addr = (offset / erase_size) * erase_size;
		/*
		|-------|-------|---------|
		     |<----data---->|
		    offset          end
		*/
		erase_align_ofs = offset % erase_size;
		erase_align_size = erase_size - erase_align_ofs;
		erase_align_size = erase_align_size > len ? len : erase_align_size;

		/*read data from flash*/
		if (log_ret(sf_get_ops(dev->parent)->read(dev->parent, erase_align_addr, erase_size, align_buf))) {
			printf("read error\n");
			goto __err;
		}

		i = 0;

		while (*(align_buf + erase_align_ofs + i) == 0xff) {
			i++;
			if (i == erase_align_size) {
				if (log_ret(sf_get_ops(dev->parent)->write(dev->parent, offset, erase_align_size, buf))) {
					printf("write error\n");
					goto __err;
				}
				goto write_complete;
			}
		}

		/* Erase the entire sector */
		if (log_ret(sf_get_ops(dev->parent)->erase(dev->parent, erase_align_addr, flash->sector_size))) {
			printf("erase error\n");
			goto __err;
		}
		/*fill data to write*/
		memcpy(align_buf + erase_align_ofs, buf, erase_align_size);

		/* write 1 sector */
		if (log_ret(sf_get_ops(dev->parent)->write(dev->parent, erase_align_addr, erase_size, align_buf))) {
			printf("write error\n");
			goto __err;
		}
write_complete:
		free(align_buf);

		/* update info */
		len -= erase_align_size;
		offset += erase_align_size;
		buf += erase_align_size;
	}
	if (len)
		ret = _spi_flash_update(dev, flash, offset, len, buf);
	return ret == 0 ? blkcnt : 0;

__err:
	if (align_buf)
		free(align_buf);
	return 0;
}

static unsigned long sunxi_flash_spinor_erase(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt)
{
	u32 offset = (start + DEFAULT_SPINOR_LOGICAL_OFFSET) << 9;
	size_t len = blkcnt << 9;
	if (blkcnt == 0)
		return 0;

	if (log_ret(sf_get_ops(dev->parent)->erase(dev->parent, offset, len)))
		return 0;
	else
		return blkcnt;
}

static int sunxi_flash_spinor_flush(struct udevice *dev)
{
	return 0;
}

static uint sunxi_flash_spinor_get_size(struct udevice *dev)
{
	struct spi_flash *flash = dev_get_uclass_priv(dev->parent);
	int blksz = 512;
	lbaint_t lba = lldiv(flash->mtd.size, blksz);

	return lba;
}

static uint sunxi_flash_spinor_get_logical_offset(struct udevice *dev)
{
	return DEFAULT_SPINOR_LOGICAL_OFFSET;
}

static uint sunxi_flash_spinor_get_storage(struct udevice *dev)
{
	return STORAGE_NOR;
}

static int sunxi_flash_spinor_phyread(struct udevice *dev, lbaint_t start,
			      lbaint_t blkcnt, void *buffer)
{
	u32 offset = start << 9;
	size_t len = blkcnt << 9;
	void *buf = buffer;
	if (blkcnt == 0)
		return 0;

	if (log_ret(sf_get_ops(dev->parent)->read(dev->parent, offset, len, buf)))
		return 0;
	else
		return blkcnt;
}

static int sunxi_flash_spinor_phywrite(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt, void *buffer)
{
	u32 offset = start << 9;
	size_t len = blkcnt << 9;
	void *buf = buffer;
	if (blkcnt == 0)
		return 0;

	if (log_ret(sf_get_ops(dev->parent)->write(dev->parent, offset, len, buf)))
		return 0;
	else
		return blkcnt;
}

static int sunxi_flash_spinor_phyerase(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt, void *skip)
{
	u32 offset = start << 9;
	size_t len = blkcnt << 9;
	if (blkcnt == 0)
		return 0;

	if (log_ret(sf_get_ops(dev->parent)->erase(dev->parent, offset, len)))
		return 0;
	else
		return blkcnt;
}

static unsigned long sunxi_flash_spinor_write(struct udevice *dev, lbaint_t start,
			       lbaint_t blkcnt, void *buffer)
{
	return _sunxi_flash_spinor_write(dev, DEFAULT_SPINOR_LOGICAL_OFFSET + start, blkcnt, buffer);
}

void spinor_update_boot0_param(void *info)
{
	struct spi_flash *flash = get_spinor();
	struct spi_slave *slave = flash->spi;
#if defined(CONFIG_AW_SPIF)
	struct sunxi_spif *sspi = dev_get_priv(slave->dev->parent);
#else
	struct sunxi_spi *sspi = dev_get_priv(slave->dev->parent);
#endif
	boot_spinor_info_t *boot_info = (boot_spinor_info_t *)info;

	strncpy((char *)boot_info->magic, (const char *)SPINOR_BOOT_PARAM_MAGIC,
			sizeof(boot_info->magic));
	boot_info->readcmd = flash->read_opcode;
	boot_info->flash_size = flash->size / 1024 / 1024;
	boot_info->erase_size = flash->erase_size;
	boot_info->read_proto = flash->read_proto;
	boot_info->write_proto = flash->write_proto;
	boot_info->read_dummy = flash->read_dummy;

	boot_info->frequency = sspi->speed_hz;
	boot_info->sample_mode = sspi->sample_mode;
	boot_info->sample_delay = sspi->sample_delay;

	if (flash->read_proto == SNOR_PROTO_1_1_4)
		boot_info->read_mode = 4;
	else if (flash->read_proto == SNOR_PROTO_1_1_2)
		boot_info->read_mode = 2;
	else
		boot_info->read_mode = 1;

	if (flash->write_proto == SNOR_PROTO_1_1_4)
		boot_info->write_mode = 4;
	else if (flash->write_proto == SNOR_PROTO_1_1_2)
		boot_info->write_mode = 2;
	else
		boot_info->write_mode = 1;

	if (flash->info->flags & SPI_NOR_4B_OPCODES)
		boot_info->addr4b_opcodes = 1;
	boot_info->bootparam_addr = DEFAULT_SPINOR_UBOOT_OFFSET - CONFIG_SPINOR_PARAM_SPACE_SIZE;
	boot_info->io_driving_level = 0x1111111;
	boot_info->csd_val = 0x50606;

	dump_spinor_info(boot_info);
}

int update_boot_param(void)
{
	struct spi_flash *flash = get_spinor();
	struct mtd_info *mtd = &flash->mtd;
	struct spi_slave *slave = flash->spi;
#if defined(CONFIG_AW_SPIF)
	struct sunxi_spif *sspi = dev_get_priv(slave->dev->parent);
#else
	struct sunxi_spi *sspi = dev_get_priv(slave->dev->parent);
#endif
	u8 erase_opcode = flash->erase_opcode;
	uint32_t erasesize = flash->mtd.erasesize;
	struct sunxi_boot_param_region *boot_param = gd->sunxi_boot_param_addr;

	if (!boot_param) {
		printf("%s() gd->boot_param is NULL\n", __func__);
		return -1;
	}

	boot_spinor_info_t *boot_info =
		(boot_spinor_info_t *)boot_param->spiflash_info;

	strncpy((char *)boot_info->magic, (const char *)SPINOR_BOOT_PARAM_MAGIC,
			sizeof(boot_info->magic));
	boot_info->readcmd = flash->read_opcode;
	boot_info->flash_size = flash->size / 1024 / 1024;
	boot_info->erase_size = flash->erase_size;
	boot_info->read_proto = flash->read_proto;
	boot_info->write_proto = flash->write_proto;
	boot_info->read_dummy = flash->read_dummy;

	boot_info->frequency = sspi->speed_hz;
	printf("boot_info->frequency:%x \n", boot_info->frequency);
	boot_info->sample_mode = sspi->sample_mode;
	boot_info->sample_delay = sspi->sample_delay;

	if (flash->read_proto == SNOR_PROTO_1_1_4)
		boot_info->read_mode = 4;
	else if (flash->read_proto == SNOR_PROTO_1_1_2)
		boot_info->read_mode = 2;
	else
		boot_info->read_mode = 1;

	if (flash->write_proto == SNOR_PROTO_1_1_4)
		boot_info->write_mode = 4;
	else if (flash->write_proto == SNOR_PROTO_1_1_2)
		boot_info->write_mode = 2;
	else
		boot_info->write_mode = 1;

	if (flash->info->flags & SPI_NOR_4B_OPCODES)
		boot_info->addr4b_opcodes = 1;

	boot_info->bootparam_addr = DEFAULT_SPINOR_UBOOT_OFFSET - CONFIG_SPINOR_PARAM_SPACE_SIZE;
	boot_info->io_driving_level = 0x1111111;
	boot_info->csd_val = 0x50606;

	boot_param->header.check_sum = sunxi_generate_checksum(
		boot_param, sizeof(struct sunxi_boot_param_region), 1,
		boot_param->header.check_sum);

	/*
	 * To not break boot0, switch bits 4K erasing
	 */
	if (flash->mtd.size > SZ_16M)
		flash->erase_opcode = SPINOR_OP_BE_4K_4B;
	else
		flash->erase_opcode = SPINOR_OP_BE_4K;
	flash->mtd.erasesize = 4096;
	flash->erase_size = 4096;
	flash->sector_size = 4096;

	struct erase_info instr;
	size_t retlen = 0;
	instr.addr = (DEFAULT_SPINOR_UBOOT_OFFSET - CONFIG_SPINOR_PARAM_SPACE_SIZE) << 9;
	instr.len = CONFIG_SPINOR_PARAM_SPACE_SIZE << 9;
	mtd->_erase(mtd, &instr);
	mtd->_write(mtd, instr.addr, instr.len, &retlen, (const u_char *)boot_param);

	flash->erase_opcode = erase_opcode;
	flash->mtd.erasesize = erasesize;
	flash->erase_size = erasesize;
	flash->sector_size = erasesize;

	return instr.len == retlen ? 0 : -1;
}

static int sunxi_sprite_spinor_download_boot_param(struct udevice *dev)
{
	if (CONFIG_SPINOR_PARAM_SPACE_SIZE && update_boot_param()) {
		pr_err("%s: write boot_param failed\n", __func__);
		return -1;
	}

	return 0;
}

static int sunxi_flash_spinor_download_spl(struct udevice *dev,
		unsigned char *buf, int len, unsigned int ext)
{

	if (len / 512 > (DEFAULT_SPINOR_UBOOT_OFFSET - CONFIG_SPINOR_PARAM_SPACE_SIZE)) {
		printf("boot0 last sector :0x%x, over write sector 0x%x\n"
		       "stop boot0 download\n",
		       len / 512, (DEFAULT_SPINOR_UBOOT_OFFSET - CONFIG_SPINOR_PARAM_SPACE_SIZE));
		return -1;
	}

#ifdef CONFIG_SUNXI_BOOT_PARAM
	struct sunxi_boot_param_region *boot_param = gd->sunxi_boot_param_addr;
	int boot0_checksum = 0;

	/* update checksum */
	if (!sunxi_bootparam_format(boot_param)) {
		if (sunxi_sprite_get_boot0_cheksum(buf, &boot0_checksum))
			return -1;
		pr_debug("boot0_checksum 0x%x \n", boot0_checksum);
		sunxi_bootparam_set_boot0_checksum(boot_param, boot0_checksum);
	}
#endif

	if (CONFIG_SPINOR_PARAM_SPACE_SIZE)
		if (update_boot_param())
			printf("update boot param error\n");

	return (len/512) == _sunxi_flash_spinor_write(dev, 0, len/512, buf) ? 0 : -1;

}

static int sunxi_flash_spinor_download_toc(struct udevice *dev,
		unsigned char *buf, int len, unsigned int ext)
{
	if (len / 512 + DEFAULT_SPINOR_UBOOT_OFFSET >
	    sunxi_flash_spinor_get_logical_offset(dev)) {
		printf("toc last block :0x%x, over write logical sector starts at block:0x%x\n"
		       "stop toc download\n",
		       DEFAULT_SPINOR_UBOOT_OFFSET + len / 512,
		       sunxi_flash_spinor_get_logical_offset(dev));
		return -1;
	}

	return (len/512) == _sunxi_flash_spinor_write(dev, DEFAULT_SPINOR_UBOOT_OFFSET, len/512, buf) ? 0 : -1;
}

unsigned long sunxi_flash_spinor_secure_storage_read(struct udevice *dev,
		int item, void *buf, unsigned int len)
{
	uint start_block = 0;

	start_block = (item * 8) + DEFAULT_SPINOR_LOGICAL_OFFSET - CONFIG_SPINOR_SECURE_STORAGE_SIZE;
	return _sunxi_flash_spinor_read(dev, start_block, len/512, (void *)buf);
}

unsigned long sunxi_flash_spinor_secure_storage_write(struct udevice *dev,
		int item, void *buf, unsigned int len)
{
	struct spi_flash *flash = dev_get_uclass_priv(dev->parent);
	u8 erase_opcode = flash->erase_opcode;
	uint32_t erasesize = flash->mtd.erasesize;
	uint start_block = 0;
	int ret = 0;

	start_block = (item * 8) + DEFAULT_SPINOR_LOGICAL_OFFSET - CONFIG_SPINOR_SECURE_STORAGE_SIZE;

	/*
	 * To not break boot0, switch bits 4K erasing
	 */
	if (flash->mtd.size > SZ_16M)
		flash->erase_opcode = SPINOR_OP_BE_4K_4B;
	else
		flash->erase_opcode = SPINOR_OP_BE_4K;
	flash->mtd.erasesize = 4096;
	flash->erase_size = 4096;
	flash->sector_size = 4096;

	ret = _sunxi_flash_spinor_write(dev, start_block, len/512, buf);

	flash->erase_opcode = erase_opcode;
	flash->mtd.erasesize = erasesize;
	flash->erase_size = erasesize;
	flash->sector_size = erasesize;

	return len/512 == ret ? 0 : -1;
}

static const struct sunxi_flash_ops sunxi_flash_spinor_ops = {
	.exit = NULL,
	.read = sunxi_flash_spinor_read,
	.write = sunxi_flash_spinor_write,
	.erase = sunxi_flash_spinor_erase,
	.force_erase = NULL,
	.flush = sunxi_flash_spinor_flush,
	.size = sunxi_flash_spinor_get_size,
	.get_logical_offset = sunxi_flash_spinor_get_logical_offset,
	.get_storage = sunxi_flash_spinor_get_storage,
	.phyread = sunxi_flash_spinor_phyread,
	.phywrite = sunxi_flash_spinor_phywrite,
	.phyerase = sunxi_flash_spinor_phyerase,
	.download_spl = sunxi_flash_spinor_download_spl,
	.download_boot_param = sunxi_sprite_spinor_download_boot_param,
	.download_toc = sunxi_flash_spinor_download_toc,
	.upload_toc = NULL,
	.write_end = NULL,
	.erase_area = NULL,
	.recover_boot0_copy0 = NULL,
	.update_backup_toc0 = NULL,
#if CONFIG_IS_ENABLED(SUNXI_PRIVATE_STORAGE)
	.pristorage_read = sunxi_flash_spinor_secure_storage_read,
	.pristorage_write = sunxi_flash_spinor_secure_storage_write,
	.secstorage_flush = NULL,
	.secstorage_fast_write = NULL,
#endif
};

U_BOOT_DRIVER(sunxi_spi_nor) = {
	.name		= "sunxi_spi_nor",
	.id		= UCLASS_SUNXI_FLASH,
	.ops		= &sunxi_flash_spinor_ops,
	.flags		= DM_FLAG_OS_PREPARE,
};

static int sunxi_flash_spinor_ops_init(struct udevice *dev)
{
	int ret = 0;
	struct udevice *bdev;
	ret = sunxi_flash_create_devicef(dev, "sunxi_spi_nor", "sunxi_flash",
					UCLASS_SPI_FLASH, dev_seq(dev), &bdev);

	if (ret) {
		pr_err("Cannot create sunxi flash spinor device\n");
		return ret;
	}

	return 0;
}

unsigned long sunxi_spinor_init(void)
{
	struct spi_flash *flash;
#if CONFIG_IS_ENABLED(DM_SPI_FLASH)
	struct udevice *new;
	int     ret;

	/* speed and mode will be read from DT */
	ret = spi_flash_probe_bus_cs(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS, &new);
	if (ret) {
		pr_err("spi flash probe error\n");
		return ret;
	}

	ret = sunxi_flash_spinor_ops_init(new);
	if (ret)
		return ret;

	flash = dev_get_uclass_priv(new);
#else
	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
					CONFIG_SF_DEFAULT_SPEED, CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		pr_err("spi flash probe error\n");
		return -EIO;
	}
#endif

	return 0;
}
