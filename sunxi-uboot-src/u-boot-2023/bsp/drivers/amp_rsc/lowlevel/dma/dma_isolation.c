/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019-2025 Allwinner Technology Co., Ltd. ALL rights reserved.
 *
 * Allwinner is a trademark of Allwinner Technology Co.,Ltd., registered in
 * the People's Republic of China and other countries.
 * All Allwinner Technology Co.,Ltd. trademarks are used with permission.
 *
 * DISCLAIMER
 * THIRD PARTY LICENCES MAY BE REQUIRED TO IMPLEMENT THE SOLUTION/PRODUCT.
 * IF YOU NEED TO INTEGRATE THIRD PARTY'S TECHNOLOGY (SONY, DTS, DOLBY, AVS OR MPEGLA, ETC.)
 * IN ALLWINNERS'SDK OR PRODUCTS, YOU SHALL BE SOLELY RESPONSIBLE TO OBTAIN
 * ALL APPROPRIATELY REQUIRED THIRD PARTY LICENCES.
 * ALLWINNER SHALL HAVE NO WARRANTY, INDEMNITY OR OTHER OBLIGATIONS WITH RESPECT TO MATTERS
 * COVERED UNDER ANY REQUIRED THIRD PARTY LICENSE.
 * YOU ARE SOLELY RESPONSIBLE FOR YOUR USAGE OF THIRD PARTY'S TECHNOLOGY.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY ALLWINNER"AS IS" AND TO THE MAXIMUM EXTENT
 * PERMITTED BY LAW, ALLWINNER EXPRESSLY DISCLAIMS ALL WARRANTIES OF ANY KIND,
 * WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING WITHOUT LIMITATION REGARDING
 * THE TITLE, NON-INFRINGEMENT, ACCURACY, CONDITION, COMPLETENESS, PERFORMANCE
 * OR MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 * IN NO EVENT SHALL ALLWINNER BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS, OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <asrm_lowlevel.h>
#ifdef AMP_SYS_RSC_MANAGER_ON_UBOOT
#include <sunxi_log.h>
#endif
#ifdef CONFIG_SUNXI_SMCCC_PERIPH_ACCESS
#include <smc.h>
#endif
#include "dma_isolation.h"

/* chan_mask range: 0x00 ~ 0x1F */
#define MAX_CHAN_MASK 0x1F

static int get_dma_id_from_addr(int addr)
{
	if (addr == SUNXI_DMAC0_BASE)
		return 0;
	else if (addr == SUNXI_DMAC1_BASE)
		return 1;
	else
		return -1;
}

#ifdef CONFIG_SUNXI_SMCCC_PERIPH_ACCESS
static int smc_dma_hw_rsc_readl(uint32_t addr)
{
	struct arm_smccc_res res = {0};
	struct sunxi_smc_periph_access_args args = {0};

	args.a1 = SUNXI_SMC_PERIPH_DMA;
	args.a2 = SUNXI_SMC_PERIPH_ACTION_READ;
	args.a3 = addr;
	sunxi_smc_periph_access(&args, &res);
	if (res.a0) {
		pr_err("smc dma_hw_rsc readl failed\n");
		return -1;
	}

	return (int)res.a1;
}

static int smc_dma_hw_rsc_writel(uint32_t val, uint32_t addr)
{
	struct arm_smccc_res res = {0};
	struct sunxi_smc_periph_access_args args = {0};

	args.a1 = SUNXI_SMC_PERIPH_DMA;
	args.a2 = SUNXI_SMC_PERIPH_ACTION_WRITE;
	args.a3 = addr;
	args.a4 = val;
	sunxi_smc_periph_access(&args, &res);
	if (res.a0 != 0) {
		pr_err("smc dma_hw_rsc writel failed\n");
		return -1;
	}

	return 0;
}
#endif /* CONFIG_SUNXI_SMCCC_PERIPH_ACCESS */

static inline uint32_t dma_hw_rsc_iso_readl(uint32_t addr)
{
	uint32_t val;

#if ((defined AMP_SYS_RSC_MANAGER_ON_UBOOT) && (defined CONFIG_SUNXI_SMCCC_PERIPH_ACCESS))
	val = smc_dma_hw_rsc_readl(addr);
#else
	val = asrm_port_readl((void *)addr);
#endif
	return val;
}

static inline int dma_hw_rsc_iso_writel(uint32_t val, uint32_t addr)
{
#if ((defined AMP_SYS_RSC_MANAGER_ON_UBOOT) && (defined CONFIG_SUNXI_SMCCC_PERIPH_ACCESS))
	return smc_dma_hw_rsc_writel(val, addr);
#else
	return asrm_port_writel(val, (void *)addr);
#endif
}

/* Helper function to check if a value is in the array */
static int is_in_array(int x, int *cpu_ic_id, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		if (*cpu_ic_id++ == x)
			return 1;
	}

	return 0;
}

static int dma_check_group_id_legal(int *cpu_ic_id, int num, int *chan_mask, int *master_id)
{
	int i;
	int consistent;
	uint8_t x;
	int exclusive;

	if (!cpu_ic_id || num <= 0) {
		sunxi_err(NULL, "%s:%d Invalid input parameters\n", __func__, __LINE__);
		return -1;
	}

	for (*chan_mask = 0; *chan_mask <= MAX_CHAN_MASK; *chan_mask = *chan_mask + 1) {
		*master_id = (cpu_ic_id[0] & *chan_mask);

		/* Check consistency: all elements in A must produce the same master_id */
		consistent = 1;
		for (i = 0; i < num; i++) {
			uint8_t val = cpu_ic_id[i];
			uint8_t result = val & *chan_mask;
			if (result != *master_id) {
				consistent = 0;
				break;
			}
		}

		if (!consistent)
			continue;

		/* Check exclusivity: all values not in A must NOT produce this master_id */
		exclusive = 1;
		for (x = 0; x <= MAX_CHAN_MASK; x++) {
			if (!is_in_array(x, cpu_ic_id, num)) {
				uint8_t result = x & *chan_mask;
				if (result == *master_id) {
					exclusive = 0;
					break;
				}
			}
		}

		if (exclusive)
			return 0;
	}

	return -1;
}

static int sunxi_dma_chan_group_set(int dma_id, struct dma_rsc_msg *rsc_msg)
{
	int i;
	int err;
	int base;
	int chan_id;

	int master_id;
	int master_id_val = 0;
	int dma_master_id_cfg_addr;

	int master_mask;
	int master_mask_val[MAX_DMA_MASK_REG_NUM] = {0};
	int dma_master_mask_cfg_addr[MAX_DMA_MASK_REG_NUM];

	sunxi_debug(NULL, "enter %s\n", __func__);
	sunxi_debug(NULL, "dma:%d\n", dma_id);

	/* fill reg addr */
	base = dma_base[dma_id];
	dma_master_id_cfg_addr = base + DMA_MASTER_ID_CFG_OFFSET;
	for (i = 0; i < MAX_DMA_MASK_REG_NUM; i++) {
		dma_master_mask_cfg_addr[i] = base
			+ DMA_MASTER_MASK_CFG_OFFSET
			+ DMA_MASTER_MASK_REG_OFFSET(i);
	}

	/* get reg val from all chan msg */
	for (chan_id = 0; chan_id < dma_max_chan[dma_id]; chan_id++) {
		master_id = rsc_msg->dma_chan_msgs[chan_id].master_id;
		master_mask = rsc_msg->dma_chan_msgs[chan_id].master_mask;

		master_mask_val[CHAN_REG_OFFSET(chan_id)] |= (master_mask << DMA_MASTER_MASK_BIT_OFFSET(chan_id));
		if (master_id < 0)
			continue;
#if defined(MASTER_ID_NEGATE)
		master_id_val |= ((!master_id) << DMA_MASTER_ID_CFG_BIT_OFFSET(chan_id));
#else
		master_id_val |= (master_id) << DMA_MASTER_ID_CFG_BIT_OFFSET(chan_id));
#endif
		sunxi_debug(NULL, "chan:%d, master_id:%d, master_mask:0x%x\n", chan_id,
		master_id, master_mask);
	}

	/* write to id cfg reg */
	err = dma_hw_rsc_iso_writel(master_id_val, dma_master_id_cfg_addr);
	if (err)
		sunxi_debug(NULL, "addr:0x%x exceed dma secure region or already write\n", dma_master_id_cfg_addr);
	sunxi_debug(NULL, "master_id_val:%x\n", master_id_val);

	/* write to mask cfg reg */
	for (i = 0; i < dma_max_mask_num[dma_id]; i++) {
		err = dma_hw_rsc_iso_writel(master_mask_val[i], dma_master_mask_cfg_addr[i]);
		if (err)
			sunxi_debug(NULL, "addr:0x%x exceed dma secure region or already write\n", dma_master_mask_cfg_addr[i]);
		sunxi_debug(NULL, "set reg:0x%x, val:0x%x\n",
				dma_master_mask_cfg_addr[i], master_mask_val[i]);
	}

	return 0;
}

void get_user_id_from_val(int dma_master_id_cfg_val, int dma_chan_mask_val, int *user_id_list, unsigned int *user_cnt)
{
	int i;
	int user_id;
	int idx = 0;

	if (!user_cnt)
		return;

	if ((dma_chan_mask_val & ~0x1F) != 0 ||
			(dma_master_id_cfg_val & ~0x1F) != 0) {
		memset(user_id_list, 0, DMA_MAX_CPU_NUM * sizeof(int));
		*user_cnt = 0;
		return;
	}

	for (i = 0; i < DMA_MAX_CPU_NUM; i++) {
		user_id = dma_cpu_ic_id[i];

		if ((user_id & dma_chan_mask_val) == dma_master_id_cfg_val) {
			if (idx < DMA_MAX_CPU_NUM)
				user_id_list[idx++] = dma_cpu_soft_id[i];
			else
				break;
		}
	}
	*user_cnt = idx;

	return;
}


static int dma_iso_enable(hw_isolator_dev_t *idev)
{
	int ret;
	struct dma_rsc_msg *rsc_msg;

	sunxi_debug(NULL, "enter %s\n", __func__);
	rsc_msg = hw_isolator_dev_get_drvdata(idev);
	ret = sunxi_dma_chan_group_set(rsc_msg->dma_id, rsc_msg);
	sunxi_debug(NULL, "exit %s\n", __func__);

	return ret;
}

static int dma_iso_disable(hw_isolator_dev_t *idev)
{
	sunxi_debug(NULL, "enter %s\n", __func__);
	sunxi_debug(NULL, "exit %s\n", __func__);

	return 0;
}

static int dma_iso_set_user_group(hw_isolator_dev_t *idev, const hw_rsc_user_group_info_t *group_info)
{
	int i, j;
	int err;
	int base;
	int cpu_ic_id[DMA_MAX_CPU_NUM] = {-1};
	int dma_master_id_addr;
	int *p = cpu_ic_id;
	int dma_chan_mask_val;
	int dma_master_id_val;
	struct dma_rsc_msg *rsc_msg;

	sunxi_debug(NULL, "enter %s\n", __func__);
	rsc_msg = hw_isolator_dev_get_drvdata(idev);

	/* init cpu_ic_id */
	for (i = 0; i < DMA_MAX_CPU_NUM; i++)
		cpu_ic_id[i] = -1;

	/* convert cpu_ic_id */
	sunxi_debug(NULL, "id:%d,user_cnt:%d, user_id:%d,  %s:%d\n", group_info->id, group_info->user_cnt, *(group_info->user_id), __func__, __LINE__);
	for (i = 0; i < group_info->user_cnt; i++) {
		for (j = 0; j < DMA_MAX_CPU_NUM; j++) {
			if (group_info->user_id[i] == dma_cpu_soft_id[j]) {
				*p++ = dma_cpu_ic_id[j];
				break;
			}
		}
	}

	for (i = 0; i < DMA_MAX_CPU_NUM; i++) {
		if (cpu_ic_id[i] != -1)
			sunxi_debug(NULL, "num %d cpu id is 0x%x, %s:%d\n", i, cpu_ic_id[i], __func__, __LINE__);
	}

	/* check legality of cpu_ic_id and count dma_master_id_val */
	err = dma_check_group_id_legal(cpu_ic_id, group_info->user_cnt, &dma_chan_mask_val, &dma_master_id_val);
	if (err == -1) {
		sunxi_err(NULL, "group inlegal %s:%d\n", __func__, __LINE__);
		return err;
	}
	dma_master_mask_list[group_info->id] = dma_chan_mask_val;
	sunxi_debug(NULL, "group legal, dma_master_id_val is 0x%x, %s:%d\n", dma_master_id_val, __func__, __LINE__);

	base = dma_base[rsc_msg->dma_id];

	if (group_info->id == 0)
		full_dma_master_id_val = 0;

#if defined(MASTER_ID_NEGATE)
	/*
	 * This macro is used to negate the passed master id.
	 * Such as SUN8IW22, the user id 0 passed in represents rv user 1 code cpux,
	 * but in the spec, master0 corresponds to cpux and master1 corresponds to rv,
	 * so the passed in user id needs to be inverted.
	 */
	full_dma_master_id_val |= dma_master_id_val << DMA_MASTER_ID_BIT_OFFSET(!(group_info->id));
#else
	full_dma_master_id_val |= dma_master_id_val << DMA_MASTER_ID_BIT_OFFSET(group_info->id);
#endif
	if (group_info->id < MAX_GROUP_NUM - 1)
		return 0;

	dma_master_id_addr = base + DMA_MASTER_ID_OFFSET;
	err = dma_hw_rsc_iso_writel(full_dma_master_id_val, dma_master_id_addr);
	if (err)
		sunxi_debug(NULL, "addr:0x%x exceed dma secure region or already write\n", dma_master_id_addr);
	sunxi_debug(NULL, "set reg:0x%x, val:0x%x\n", dma_master_id_addr, full_dma_master_id_val);

	sunxi_debug(NULL, "exit %s\n", __func__);

	return 0;
}
static int dma_iso_get_user_group(hw_isolator_dev_t *idev, hw_rsc_user_group_info_t *group_info)
{
	int i;
	int base;
	int dma_master_id_cfg_addr;
	int dma_master_id_cfg_val;
	int dma_chan_mask_val;
	struct dma_rsc_msg *rsc_msg;

	sunxi_debug(NULL, "enter %s\n", __func__);
	rsc_msg = hw_isolator_dev_get_drvdata(idev);

	base = dma_base[rsc_msg->dma_id];
	dma_master_id_cfg_addr = base + DMA_MASTER_ID_OFFSET;

	dma_master_id_cfg_val = dma_hw_rsc_iso_readl(dma_master_id_cfg_addr);
	sunxi_debug(NULL, "get reg:0x%x, val:0x%x\n", dma_master_id_cfg_addr, dma_master_id_cfg_val);
	dma_master_id_cfg_val = dma_master_id_cfg_val >> DMA_MASTER_ID_BIT_OFFSET(group_info->id);
	dma_master_id_cfg_val &= DMA_MASTER_ID_BIT_MASK;
	dma_chan_mask_val = dma_master_mask_list[group_info->id];

	group_info->user_id = asrm_port_malloc(DMA_MAX_CPU_NUM * sizeof(hw_rsc_user_id_t));
	if (!group_info->user_id) {
		asrm_err("mem allocation for DMA isolator failed\n");
		return -1;
	}

	get_user_id_from_val(dma_master_id_cfg_val, dma_chan_mask_val, group_info->user_id, &group_info->user_cnt);

	for (i = 0; i < group_info->user_cnt; i++) {
		sunxi_debug(NULL, "user_id:%d,  %s:%d\n", group_info->user_id[i], __func__, __LINE__);
	}

	sunxi_debug(NULL, "exit %s\n", __func__);

	return 0;
}

static int dma_iso_set_resource_owner(hw_isolator_dev_t *idev, const hw_rsc_info_t *rsc_info, uint32_t user_group_id)
{
	int dma_id;
	reg_addr_info_t dma_base;
	struct dma_rsc_msg *rsc_msg;

	rsc_msg = hw_isolator_dev_get_drvdata(idev);
	hw_isolator_dev_get_reg_addr_info(idev, &dma_base);

	dma_id = get_dma_id_from_addr(dma_base.base_addr);
	if (dma_id < 0) {
		sunxi_err(NULL, "dma addr err!\n");
		return -1;
	}

	rsc_msg->dma_chan_msgs[rsc_info->dma.channel_id].master_id = user_group_id;
	rsc_msg->dma_chan_msgs[rsc_info->dma.channel_id].master_mask = dma_master_mask_list[user_group_id];

	return 0;
}

static int dma_iso_get_resource_owner(hw_isolator_dev_t *idev, const hw_rsc_info_t *rsc_info, uint32_t *user_group_id)
{
	int val;
	int base;
	int dma_id;
	int chan_id;
	reg_addr_info_t dma_base;
	int dma_master_id_cfg_addr;

	sunxi_debug(NULL, "enter %s\n", __func__);

	chan_id = rsc_info->dma.channel_id;

	/* get dma id */
	hw_isolator_dev_get_reg_addr_info(idev, &dma_base);
	dma_id = get_dma_id_from_addr(dma_base.base_addr);
	base = dma_base.base_addr;
	if (dma_id < 0) {
		sunxi_err(NULL, "dma addr err!\n");
		return -1;
	}
	if (chan_id >= dma_max_chan[dma_id])
		return -1;

	/* get id cfg reg val */
	dma_master_id_cfg_addr = base + DMA_MASTER_ID_CFG_OFFSET;
	val = dma_hw_rsc_iso_readl(dma_master_id_cfg_addr);
	*user_group_id = val >> DMA_MASTER_ID_CFG_BIT_OFFSET(chan_id);
	*user_group_id &= DMA_MASTER_ID_CFG_BIT_MASK;
	sunxi_debug(NULL, "chan_id:%d, master id:%d\n", chan_id, *user_group_id);

	sunxi_debug(NULL, "exit %s\n", __func__);
	return 0;
}

static const hw_isolator_dev_ops_t g_dma_iso_ops = {
	.enable = dma_iso_enable,
	.disable = dma_iso_disable,
	.set_user_group = dma_iso_set_user_group,
	.get_user_group = dma_iso_get_user_group,
	.set_resource_owner = dma_iso_set_resource_owner,
	.get_resource_owner = dma_iso_get_resource_owner,
};

static int dma_isolation_probe(hw_isolator_dev_t *idev)
{
	int dma_id;
	int chan_id;
	reg_addr_info_t dma_base;
	struct dma_rsc_msg *rsc_msg;

	hw_isolator_dev_set_ops(idev, &g_dma_iso_ops);
	hw_isolator_dev_get_reg_addr_info(idev, &dma_base);

	rsc_msg = asrm_port_malloc(sizeof(struct dma_rsc_msg));
	if (!rsc_msg) {
		asrm_err("mem allocation for DMA isolator failed\n");
		return -1;
	}
	dma_id = get_dma_id_from_addr(dma_base.base_addr);
	if (dma_id < 0) {
		return -1;
		sunxi_err(NULL, "dma addr err!\n");
	}
	rsc_msg->dma_id = dma_id;

	hw_isolator_dev_set_drvdata(idev, (void *)rsc_msg);

	for (chan_id = 0; chan_id < dma_max_chan[dma_id]; chan_id++) {
		rsc_msg->dma_chan_msgs[chan_id].chan_id = chan_id;
		rsc_msg->dma_chan_msgs[chan_id].master_id = UNKNOW_MASTER_ID;
		rsc_msg->dma_chan_msgs[chan_id].master_mask = FIX_MASTER_MASK;
	}

	dma_iso_enable_clk();

	sunxi_debug(NULL, "dma isolation probe success\n");

	return 0;
}

static int dma_isolation_remove(hw_isolator_dev_t *idev)
{
	struct dma_rsc_msg *rsc_msg;

	rsc_msg = hw_isolator_dev_get_drvdata(idev);
	asrm_port_free(rsc_msg);

	return 0;
}

static const hw_isolator_dev_info_t g_dma_iso_match[] = {
	{ .compatible = "allwinner,dma-v111" },
	{ /* sentinel */ }
};

hw_isolator_driver_t g_dma_iso_drv = {
	.probe = dma_isolation_probe,
	.remove = dma_isolation_remove,
	.match_table = g_dma_iso_match,
};
