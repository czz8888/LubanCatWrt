/*
 * (C) Copyright 2018 allwinnertech  <wangwei@allwinnertech.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#ifndef __SMC_H__
#define __SMC_H__

#include <asm/io.h>
#include <linux/arm-smccc.h>

#if IS_ENABLED(CONFIG_SUNXI_SMCCC_PERIPH_ACCESS)
struct sunxi_smc_periph_access_args {
	/* unsigned long a0 */	/* Func ID */
	unsigned long a1;	/* Module */
	unsigned long a2;	/* Action */
	unsigned long a3;
	unsigned long a4;
	unsigned long a5;
	unsigned long a6;
	unsigned long a7;
};

/* sunxi smc periph module */
#define SUNXI_SMC_PERIPH_NONE		0  /* Reserved */
#define SUNXI_SMC_PERIPH_GPIO		1
#define SUNXI_SMC_PERIPH_DMA	 	2

/* sunxi smc periph action */
#define SUNXI_SMC_PERIPH_ACTION_NONE    0  /* Reserved */
#define SUNXI_SMC_PERIPH_ACTION_READ    1
#define SUNXI_SMC_PERIPH_ACTION_WRITE   2

void sunxi_smc_periph_access(struct sunxi_smc_periph_access_args *args, struct arm_smccc_res *res);
#endif /* CONFIG_SUNXI_SMCCC_PERIPH_ACCESS */

u32 smc_readl(ulong addr);
void smc_writel(u32 val, ulong addr);


extern u32 __sunxi_smc_call(ulong arg0, ulong arg1, ulong arg2, ulong arg3);

u32 sunxi_smc_call(ulong arg0, ulong arg1, ulong arg2, ulong arg3, ulong pResult);
int arm_svc_version(u32 *major, u32 *minor);
int arm_svc_call_count(void);
int arm_svc_uuid(u32 *uuid);
int arm_svc_run_os(ulong kernel, ulong fdt, ulong arg2);
int arm_svc_run_hyper(ulong entry, ulong fdt, ulong arg2);
u32 arm_svc_read_sec_reg(ulong reg);
int arm_svc_write_sec_reg(u32 val, ulong reg);
int arm_svc_arisc_startup(ulong cfg_base);
int arm_svc_arisc_wait_ready(void);
int arm_svc_arisc_fake_poweroff(void);
int arm_svc_fake_poweroff(ulong dtb_base);
int arm_svc_poweroff(void);
int arm_svc_poweroff_charge(void);
u32 arm_svc_arisc_read_pmu(ulong addr);
int arm_svc_arisc_write_pmu(ulong addr, u32 value);

int arm_svc_efuse_read(void *key_buf, void *read_buf);
int arm_svc_efuse_write(void *key_buf);
int arm_svc_probe_secure_mode(void);
int arm_svc_customer_encrypt(u32 customer_reserved_id);


int smc_init(void);

int smc_tee_check_hash(const char *name, u8 *hash);
int smc_tee_ssk_encrypt(char *out_buf, char *in_buf, int len, int *out_len);
int smc_tee_ssk_decrypt(char *out_buf, char *in_buf, int len);
int smc_tee_rssk_encrypt(char *out_buf, char *in_buf, int len, int *out_len);
int smc_aes_rssk_decrypt_to_keysram(void);
int smc_aes_algorithm(char *out_buf, char *in_buf, int data_len, char *pkey, int key_mode, int decrypt);
int smc_tee_keybox_store(const char *name, char *in_buf, int len);
int smc_tee_probe_drm_configure(ulong *drm_base, ulong *drm_size);
int smc_tee_setup_mips(uint32_t load_addr, uint32_t mips_size);

int smc_tee_inform_fdt(uint64_t base, uint32_t size);

int arm_svc_set_cpu_on(int cpu, uint entry);
int arm_svc_set_cpu_off(int cpu);
int arm_svc_set_cpu_wfi(void);

/*for multi cluster*/
int sunxi_smc_set_cpu_entry(u32 entry, int cpu);
/*for multi cluster*/
int sunxi_smc_set_cpu_off(void);


#endif
