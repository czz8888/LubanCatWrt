/* SPDX-License-Identifier: GPL-2.0+
 * Copyright (C) 2023 Allwinnertech
*/

#include <compiler.h>
#include <asm/global_data.h>
#include <private_uboot.h>
#include <sunxi_flash.h>
#include <asm/arch/rtc.h>
#include <asm/arch/efuse.h>
#include <asm/io.h>
#include <sunxi_board.h>
#include <common.h>
#include <sysreset.h>
#include <android_image.h>
#include <image.h>
#include <malloc.h>
#include <sunxi_efuse_map.h>
#include <dm/read.h>
#include <axp_pmic.h>
#include <sunxi_image_verifier.h>
#include <smc.h>
#include <dm.h>
#include <dm/uclass-internal.h>
#include <dm/util.h>
DECLARE_GLOBAL_DATA_PTR;
#include <asm/types.h>

int  __attribute__((weak)) sunxi_platform_power_off(int status)
{
	return 0;
}

int sunxi_set_uboot_shell(int flag)
{
//	gd->uboot_shell = flag;
	return 0;
}

void set_boot_work_mode(int work_mode)
{
       uboot_spare_head.boot_data.work_mode = work_mode;
}

int get_boot_work_mode(void)
{
	return uboot_spare_head.boot_data.work_mode;
}

bool boot_from_uboot_back(void)
{
	return uboot_spare_head.boot_data.res[0] ? true : false;
}

int sunxi_probe_secure_monitor(void)
{
	return uboot_spare_head.boot_data.monitor_exist ==
			       SUNXI_SECURE_MODE_USE_SEC_MONITOR ?
		       1 :
		       0;
}

int sunxi_probe_secure_os(void)
{
	return uboot_spare_head.boot_data.secureos_exist;
}

void sunxi_dma_destroy(void)
{
	struct uclass *uc_dev;
	int ret;

	ret = uclass_get(UCLASS_DMA, &uc_dev);
	if (uc_dev)
		ret = uclass_destroy(uc_dev);
	if (ret)
		printf("could not remove dma devices");
}

/*
  * note:
  * call driver exit here.
  * this func be called before enter linux.
  */
void board_quiesce_devices(void)
{
	sunxi_flash_flush();
	/*modify 2 for nor to finally exit*/
	sunxi_flash_exit(2);

#ifdef CONFIG_AW_DMA
	sunxi_dma_destroy();
#endif
}

void sunxi_board_close_source(void)
{
	board_quiesce_devices();
	disable_interrupts();
	return;
}

int sunxi_board_run_fel(void)
{
	rtc_set_fel_flag();
	sunxi_board_close_source();
	reset_cpu();
	return 0;
}

int sunxi_get_secureboard(void)
{
#ifdef SID_SECURE_MODE
	return readl(IOMEM_ADDR(SID_SECURE_MODE)) & 1;
#elif defined(EFUSE_ANTI_BRUSH)
	return (readl(ANTI_BRUSH_MODE) >> ANTI_BRUSH_BIT_OFFSET) & 1;
#elif defined(SECURE_READ_TEST_REG)
	/*
	 * two way start up uboot:
	 * 1.fel:
	 *		board secure = ~(cpu secure)
	 * 2.from optee(boot0/sboot)
	 *		optee help read secure enable bit
	 */
	if (sunxi_probe_secure_os()) {
		return (((arm_svc_read_sec_reg(SUNXI_SID_SRAM_BASE + EFUSE_LCJS)) &
					(1 << 11)) != 0);
	} else {
		return (readl(SECURE_READ_TEST_REG) == 0);
	}
#else
	return 0;
#endif
}

int sunxi_probe_securemode(void)
{
	int secure_mode = 0;

	secure_mode = sunxi_get_secureboard();
	pr_notice("secure enable bit: %d\n", secure_mode);

	if (secure_mode) {
		// sbrom  set  secureos_exist flag,
		// 1: secure os exist 0: secure os not exist
		if (uboot_spare_head.boot_data.secureos_exist == 1) {
			gd->securemode = SUNXI_SECURE_MODE_WITH_SECUREOS;
			pr_debug("secure mode: with secureos\n");
		} else {
			gd->securemode = SUNXI_SECURE_MODE_NO_SECUREOS;
			pr_debug("secure mode: no secureos\n");
		}
		gd->bootfile_mode = SUNXI_BOOT_FILE_TOC;
#ifdef CONFIG_SUNXI_ANTI_BRUSH
		debug("init preserve toc1\n");
		if (sunxi_verify_preserve_toc1((void *)CONFIG_SUNXI_BOOTPKG_BASE)) {
			pr_err("%s: preserve toc1 error\n", __func__);
		}
#endif
	} else {
		//boot0  set  secureos_exist flag,
		//1: secure monitor exist 0: secure monitor  not exist
		int burn_secure_mode = 0;

		gd->securemode = SUNXI_NORMAL_MODE;
		gd->bootfile_mode = SUNXI_BOOT_FILE_PKG;

		if (get_boot_work_mode() != WORK_MODE_BOOT) {
			debug("check if downloading secure img\n");
			burn_secure_mode = uboot_spare_head.boot_data.secure_mode;

			if (burn_secure_mode != 1)
				return 0;

			printf("normal mode: download secure firmware\n");
			gd->bootfile_mode = SUNXI_BOOT_FILE_TOC;
		}
	}
	return 0;
}

#if defined(CONFIG_SUNXI_BURN_ROTPK_ON_SPRITE) || \
	defined(CONFIG_SUNXI_ROTPK_BURN_ENABLE_BY_TOOL)
int sunxi_burn_rotpk(void)
{
	u8 hash[32] = { 0 }, readback_hash[32] = { 0 }, hash_zero[32] = { 0 };
	int hash_len = 0;
	efuse_key_info_t efuse_key_info;

#if defined(CONFIG_SUNXI_ROTPK_BURN_ENABLE_BY_TOOL)
	if ((uboot_spare_head.boot_data.func_mask &
	     UBOOT_FUNC_MASK_BIT_BURN_ROTPK) !=
	    UBOOT_FUNC_MASK_BIT_BURN_ROTPK) {
		pr_err("tool did not set rotpk burn flag, skip rotpk burn\n");
		/* tool did not set uboot to burn rotpk, do not burn */
		return 0;
	}
#endif

	if (sunxi_verify_get_rotpk_hash(hash)) {
		pr_err("get rotpk failed\n");
		return -1;
	}

	memset(&efuse_key_info, 0, sizeof(efuse_key_info));
	efuse_key_info.len = 32;
	memcpy(efuse_key_info.name, "rotpk", sizeof("rotpk"));
	efuse_key_info.key_data = hash;

	memset(readback_hash, 0, 32);
	if (gd->securemode == SUNXI_NORMAL_MODE) {
		sunxi_efuse_read_dm("rotpk", readback_hash, &hash_len);
	} else {
#if IS_ENABLED(CONFIG_OPTEE25)
		arm_svc_efuse_read("rotpk", readback_hash);
#else
		sunxi_efuse_read_dm("rotpk", readback_hash, &hash_len);
#endif
	}

	printf("read rotpk before write:\n");
	sunxi_dump(readback_hash, 32);

	if (memcmp(readback_hash, hash_zero, 32)) {
		printf("puk hash not zero\n");
		if (memcmp(readback_hash, hash, 32)) {
			printf("puk hash not same\n");
			return -1;
		} else {
			printf("puk hash is same, skip burn rotpk\n");
			return 0;
		}
	}

	memset(readback_hash, 0, 32);
	printf("now write rotpk:\n");
	if (gd->securemode == SUNXI_NORMAL_MODE) {
		if (sunxi_efuse_write_dm(&efuse_key_info)) {
			pr_err("burn rotpk failed");
			return -1;
		}
		if (sunxi_efuse_read_dm("rotpk", readback_hash, &hash_len)) {
			pr_err("read puk hash fail\n");
			return -1;
		}
	} else {
#if IS_ENABLED(CONFIG_OPTEE25)
		if (arm_svc_efuse_write(&efuse_key_info)) {
			pr_err("svc burn rotpk failed\n");
			return -1;
		}
		if (arm_svc_efuse_read("rotpk", readback_hash) != 32) {
			pr_err("svc read puk hash fail\n");
			return -1;
		}
#else
		if (sunxi_efuse_write_dm(&efuse_key_info)) {
			pr_err("burn rotpk failed");
			return -1;
		}
		if (sunxi_efuse_read_dm("rotpk", readback_hash, &hash_len)) {
			pr_err("read puk hash fail\n");
			return -1;
		}
#endif
	}

	printf("rotpk burn done, using rotpk:\n");
	sunxi_dump(readback_hash, 32);
	if (memcmp(efuse_key_info.key_data, readback_hash, 32) != 0) {
		pr_err("verify rotpk failed, firmware rotpk:\n");
		sunxi_dump(efuse_key_info.key_data, 32);
		return -1;
	}

	return 0;
}
#endif

int sunxi_set_secure_mode(void)
{
#if IS_ENABLED(CONFIG_AW_EFUSE)
	int mode;
	u8  hash[32] = {0}, hash_tmp[32] = {0};
	int hash_len = 0;

	if ((gd->securemode == SUNXI_NORMAL_MODE) &&
	    (gd->bootfile_mode == SUNXI_BOOT_FILE_TOC)) {
		mode = sid_probe_security_mode_dm();
		if (!mode) {
			int ret = sunxi_efuse_get_rotpk_status_dm();

			if (ret == -1) {
				//api not supported, try read rotpk directly
				if (sunxi_efuse_usr_read("rotpk", hash,
						     &hash_len)) {
					printf("read puk hash fail\n");
					return -1;
				}
				printf("read puk finished,len:%d\n", hash_len);
				if (memcmp(hash, hash_tmp, sizeof(hash))) {
					printf("puk hash not zero,fail\n");
					return -1;
				}
			} else if (ret == 1) {
				printf("puk burned,stop set secure mode!\n");
				return -1;
			}

#if defined(CONFIG_SUNXI_BURN_ROTPK_ON_SPRITE) || \
	defined(CONFIG_SUNXI_ROTPK_BURN_ENABLE_BY_TOOL)
			if (sunxi_burn_rotpk())
				return -1;
#endif

			if (sid_set_security_mode_dm()) {
				printf("burn secure bit fail\n");
				return -1;
			}
			gd->bootfile_mode = SUNXI_BOOT_FILE_TOC;
			printf("burn done, now secure bit is:%d\n",
			       sid_probe_security_mode_dm());
		} else {
			printf("secure chip, don't repeat burn secure bit\n");
		}
	}

	return 0;
#else
	printf("The efuse configuration is not open and cannot be burn secure bit\n");
	return -1;
#endif
}

int sunxi_get_securemode(void)
{
	return gd->securemode;
}

int sunxi_boot_image_get_embbed_cert_len(const void *hdr)
{
	struct boot_img_hdr_ex *hdr_ex = (struct boot_img_hdr_ex *)hdr;
	if (strncmp((void *)(hdr_ex->cert_magic), AW_CERT_MAGIC,
		    strlen(AW_CERT_MAGIC)) != 0)
		return 0;
	else
		return hdr_ex->cert_size;
}

int sunxi_boot_fitimage_get_embbed_cert_len(const void *hdr)
{
	struct boot_img_fdt_ex *hdr_ex = (struct boot_img_fdt_ex *)hdr;
	if (strncmp((void *)(hdr_ex->cert_magic), AW_CERT_MAGIC,
		    strlen(AW_CERT_MAGIC)) != 0)
		return 0;
	else
		return hdr_ex->cert_size;
}

int sunxi_board_restart(int next_mode)
{
    rtc_set_bootmode_flag(next_mode);
    sunxi_board_close_source();
    reset_cpu();

    return 0;
}

int sunxi_board_shutdown(void)
{
	sunxi_board_close_source();
#ifdef CONFIG_SUNXI_UBOOT_POWER_OFF
	sunxi_platform_power_off(0);
#endif
#ifdef CONFIG_AW_PMIC_AXP
	axp_set_power_off();
#endif
	while (1) {
		asm volatile ("wfi");
	}
	return 0;
}

int sunxi_board_shutdown_charge(void)
{
	sunxi_board_close_source();
#ifdef CONFIG_SUNXI_UBOOT_POWER_OFF
	sunxi_platform_power_off(1);
#endif
#ifdef CONFIG_AW_PMIC_AXP
	axp_set_power_off();
#endif
	while (1) {
		asm volatile ("wfi");
	}
	return 0;
}

u32 sunxi_generate_checksum(void *buffer, u32 length, u32 div, u32 src_sum)
{
	u32 *buf;
	int count;
	u32 sum;

	count = length >> 2;
	sum   = 0;
	buf   = (__u32 *)buffer;
	do {
		sum += *buf++;
		sum += *buf++;
		sum += *buf++;
		sum += *buf++;
	} while ((count -= (4*div)) > (4 - 1));

	while (count-- > 0)
		sum += *buf++;

	sum = sum - src_sum + STAMP_VALUE;

	return sum;
}


u32 sunxi_verify_checksum(void *buffer, u32 length, u32 src_sum)
{
	u32 sum;
	sum = sunxi_generate_checksum(buffer, length, 1, src_sum);

	debug("src sum=%x, check sum=%x\n", src_sum, sum);
	if (sum == src_sum)
		return 0;
	else
		return -1;

}

/*return 0:android image, -1:other image*/
int sunxi_probe_android_kernel(void)
{
	int ret = 0;
	uint start_block = env_get_hex("kernel_start_blk", 0x0) - sunxi_flash_get_logical_offset();
	uint head_size = ALIGN(sizeof(struct andr_img_hdr), 512); //bytes

	struct andr_img_hdr *buf = (struct andr_img_hdr *)malloc(head_size);
	if (buf == NULL)
		return ret;

	memset(buf, '0', head_size);
	sunxi_flash_read(start_block, head_size / 512, (void *)buf);

	ret = android_image_check_header(buf);

	free(buf);
	return ret;
}

void board_prep_linux(struct bootm_headers *images)
{
	if (sunxi_probe_android_kernel()) {
		pr_debug("not android kernel, update dts now\n");
		int sunxi_update_fdt_para_for_kernel(void);
		sunxi_update_fdt_para_for_kernel();
	} else {
		//android image
#ifdef CONFIG_SUNXI_ANDROID_OVERLAY
		void set_andriod_dtbo_idx(void);
		int check_dtbo_idx(void);
		void *sunxi_support_ufdt(void *dtb_base, u32 dtb_len);

		set_andriod_dtbo_idx();

		if (check_dtbo_idx() == 0) {
			if (sunxi_support_ufdt((void *)images->ft_addr, fdt_totalsize(images->ft_addr)) == NULL) {
				pr_err("sunxi android dto merge fail\n");
			}
		}
#endif
	}
}

int sunxi_set_force_32bit_os(int forced)
{
	gd->force_32bit_os = forced;
	return 0;
}

int sunxi_get_force_32bit_os(void)
{
	return gd->force_32bit_os;
}

int sunxi_get_irq(struct udevice *dev)
{
       int irq_num;

       dev_read_u32(dev, "sunxi-interrupt", &irq_num);

       return irq_num;
}

/*
 * sunxi_parsed_specific_string() - Parse the string skipped by skip_character at intervals of space_character
 * @intput_string: the string to be parsed
 * @output_para[][16]: store the parsed string
 * @space_character: characters separated by space_character
 * @skip_character: skip when encountering skip_character, 0 is invalid
 *
 * exp:
 * 	the string to be parsed:"bootloader, env,boot,vendor_boot, dtbo"
 * 	space_character = ','
 * 	skip_character = ' '
 * output:
 * 	output_para[0] = bootloader
 * 	output_para[1] = env
 * 	output_para[2] = boot
 * 	output_para[3] = vendor_boot
 * 	output_para[4] = dtbo
 *
 */
int sunxi_parsed_specific_string(char *intput_string, char output_para[][16], char space_character, char skip_character)
{
	int i, j, k;
	for (i = 0, j = 0, k = 0;; i++) {
		if ((skip_character != 0) && (intput_string[i] == skip_character)) {
			continue;
		} else if ((intput_string[i] == space_character) || (intput_string[i] == 0)) {
			output_para[k][j] = 0;
			k++;
			j = 0;
			if (intput_string[i] == 0)
				break;
		} else {
			output_para[k][j] = intput_string[i];
			j++;
		}
	}
	/* for (i = 0; i < k; i++)
	 *         printf("output_para[%d]:%s\n", i, output_para[i]); */
	return 0;
}

void sunxi_update_subsequent_processing(int next_work)
{
	printf("next work %d\n", next_work);
	switch (next_work) {
	case SUNXI_UPDATE_NEXT_ACTION_REBOOT:
	case SUNXI_UPDATA_NEXT_ACTION_SPRITE_TEST:
		printf("SUNXI_UPDATE_NEXT_ACTION_REBOOT\n");
		sunxi_board_restart(0);
		break;

	case SUNXI_UPDATE_NEXT_ACTION_SHUTDOWN:
		printf("SUNXI_UPDATE_NEXT_ACTION_SHUTDOWN\n");
#ifdef CONFIG_SUNXI_UPDATE_REMIND
		sunxi_update_remind();
#endif
		sunxi_board_shutdown();
		break;

	case SUNXI_UPDATE_NEXT_ACTION_REUPDATE:
		printf("SUNXI_UPDATE_NEXT_ACTION_REUPDATE\n");
		sunxi_board_run_fel();
		break;

	case SUNXI_UPDATE_NEXT_ACTION_BOOT:
	case SUNXI_UPDATE_NEXT_ACTION_NORMAL:
	default:
		printf("SUNXI_UPDATE_NEXT_ACTION_NULL\n");
#ifdef CONFIG_SUNXI_UPDATE_REMIND
		sunxi_update_remind();
#endif
		break;
	}

	return;
}
