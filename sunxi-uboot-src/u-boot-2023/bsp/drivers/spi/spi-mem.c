// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2018 Exceet Electronics GmbH
 * Copyright (C) 2018 Bootlin
 *
 * Author: Boris Brezillon <boris.brezillon@bootlin.com>
 */

#ifndef __UBOOT__
#include <log.h>
#include <dm/devres.h>
#include <linux/dmaengine.h>
#include <linux/pm_runtime.h>
#include "internals.h"
#else
#include <common.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <spi.h>
#include <spi.h>
#include <spi-mem.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <linux/bug.h>
#endif

#ifndef __UBOOT__
/**
 * spi_controller_dma_map_mem_op_data() - DMA-map the buffer attached to a
 *					  memory operation
 * @ctlr: the SPI controller requesting this dma_map()
 * @op: the memory operation containing the buffer to map
 * @sgt: a pointer to a non-initialized sg_table that will be filled by this
 *	 function
 *
 * Some controllers might want to do DMA on the data buffer embedded in @op.
 * This helper prepares everything for you and provides a ready-to-use
 * sg_table. This function is not intended to be called from spi drivers.
 * Only SPI controller drivers should use it.
 * Note that the caller must ensure the memory region pointed by
 * op->data.buf.{in,out} is DMA-able before calling this function.
 *
 * Return: 0 in case of success, a negative error code otherwise.
 */
int spi_controller_dma_map_mem_op_data(struct spi_controller *ctlr,
				       const struct spi_mem_op *op,
				       struct sg_table *sgt)
{
	struct device *dmadev;

	if (!op->data.nbytes)
		return -EINVAL;

	if (op->data.dir == SPI_MEM_DATA_OUT && ctlr->dma_tx)
		dmadev = ctlr->dma_tx->device->dev;
	else if (op->data.dir == SPI_MEM_DATA_IN && ctlr->dma_rx)
		dmadev = ctlr->dma_rx->device->dev;
	else
		dmadev = ctlr->dev.parent;

	if (!dmadev)
		return -EINVAL;

	return spi_map_buf(ctlr, dmadev, sgt, op->data.buf.in, op->data.nbytes,
			   op->data.dir == SPI_MEM_DATA_IN ?
			   DMA_FROM_DEVICE : DMA_TO_DEVICE);
}
EXPORT_SYMBOL_GPL(spi_controller_dma_map_mem_op_data);

/**
 * spi_controller_dma_unmap_mem_op_data() - DMA-unmap the buffer attached to a
 *					    memory operation
 * @ctlr: the SPI controller requesting this dma_unmap()
 * @op: the memory operation containing the buffer to unmap
 * @sgt: a pointer to an sg_table previously initialized by
 *	 spi_controller_dma_map_mem_op_data()
 *
 * Some controllers might want to do DMA on the data buffer embedded in @op.
 * This helper prepares things so that the CPU can access the
 * op->data.buf.{in,out} buffer again.
 *
 * This function is not intended to be called from SPI drivers. Only SPI
 * controller drivers should use it.
 *
 * This function should be called after the DMA operation has finished and is
 * only valid if the previous spi_controller_dma_map_mem_op_data() call
 * returned 0.
 *
 * Return: 0 in case of success, a negative error code otherwise.
 */
void spi_controller_dma_unmap_mem_op_data(struct spi_controller *ctlr,
					  const struct spi_mem_op *op,
					  struct sg_table *sgt)
{
	struct device *dmadev;

	if (!op->data.nbytes)
		return;

	if (op->data.dir == SPI_MEM_DATA_OUT && ctlr->dma_tx)
		dmadev = ctlr->dma_tx->device->dev;
	else if (op->data.dir == SPI_MEM_DATA_IN && ctlr->dma_rx)
		dmadev = ctlr->dma_rx->device->dev;
	else
		dmadev = ctlr->dev.parent;

	spi_unmap_buf(ctlr, dmadev, sgt,
		      op->data.dir == SPI_MEM_DATA_IN ?
		      DMA_FROM_DEVICE : DMA_TO_DEVICE);
}
EXPORT_SYMBOL_GPL(spi_controller_dma_unmap_mem_op_data);
#endif /* __UBOOT__ */

static int spi_check_buswidth_req(struct spi_slave *slave, u8 buswidth, bool tx)
{
	u32 mode = slave->mode;

	switch (buswidth) {
	case 1:
		return 0;

	case 2:
		if ((tx && (mode & (SPI_TX_DUAL | SPI_TX_QUAD))) ||
		    (!tx && (mode & (SPI_RX_DUAL | SPI_RX_QUAD))))
			return 0;

		break;

	case 4:
		if ((tx && (mode & SPI_TX_QUAD)) ||
		    (!tx && (mode & SPI_RX_QUAD)))
			return 0;

		break;
	case 8:
		if ((tx && (mode & SPI_TX_OCTAL)) ||
		    (!tx && (mode & SPI_RX_OCTAL)))
			return 0;

		break;

	default:
		break;
	}

	return -ENOTSUPP;
}

static bool spi_mem_check_buswidth(struct spi_slave *slave,
				   const struct spi_mem_op *op)
{
	if (spi_check_buswidth_req(slave, op->cmd.buswidth, true))
		return false;

	if (op->addr.nbytes &&
	    spi_check_buswidth_req(slave, op->addr.buswidth, true))
		return false;

	if (op->dummy.nbytes &&
	    spi_check_buswidth_req(slave, op->dummy.buswidth, true))
		return false;

	if (op->data.dir != SPI_MEM_NO_DATA &&
	    spi_check_buswidth_req(slave, op->data.buswidth,
				   op->data.dir == SPI_MEM_DATA_OUT))
		return false;

	return true;
}

bool spi_mem_dtr_supports_op(struct spi_slave *slave,
			     const struct spi_mem_op *op)
{
	if (op->cmd.buswidth == 8 && op->cmd.nbytes % 2)
		return false;

	if (op->addr.nbytes && op->addr.buswidth == 8 && op->addr.nbytes % 2)
		return false;

	if (op->dummy.nbytes && op->dummy.buswidth == 8 && op->dummy.nbytes % 2)
		return false;

	if (op->data.dir != SPI_MEM_NO_DATA &&
	    op->dummy.buswidth == 8 && op->data.nbytes % 2)
		return false;

	return spi_mem_check_buswidth(slave, op);
}
EXPORT_SYMBOL_GPL(spi_mem_dtr_supports_op);

bool spi_mem_default_supports_op(struct spi_slave *slave,
				 const struct spi_mem_op *op)
{
	if (op->cmd.dtr || op->addr.dtr || op->dummy.dtr || op->data.dtr)
		return false;

	if (op->cmd.nbytes != 1)
		return false;

	return spi_mem_check_buswidth(slave, op);
}
EXPORT_SYMBOL_GPL(spi_mem_default_supports_op);

/**
 * spi_mem_supports_op() - Check if a memory device and the controller it is
 *			   connected to support a specific memory operation
 * @slave: the SPI device
 * @op: the memory operation to check
 *
 * Some controllers are only supporting Single or Dual IOs, others might only
 * support specific opcodes, or it can even be that the controller and device
 * both support Quad IOs but the hardware prevents you from using it because
 * only 2 IO lines are connected.
 *
 * This function checks whether a specific operation is supported.
 *
 * Return: true if @op is supported, false otherwise.
 */
bool spi_mem_supports_op(struct spi_slave *slave,
			 const struct spi_mem_op *op)
{
	struct udevice *bus = slave->dev->parent;
	struct dm_spi_ops *ops = spi_get_ops(bus);

	if (ops->mem_ops && ops->mem_ops->supports_op)
		return ops->mem_ops->supports_op(slave, op);

	return spi_mem_default_supports_op(slave, op);
}
EXPORT_SYMBOL_GPL(spi_mem_supports_op);

#ifdef CONFIG_AW_SPIF
#include "spif-sunxi.h"
#define CONFIG_SUNXI_SPIF_V0	0x10000
#define CONFIG_SUNXI_SPIF_V1	0x10001
#define CONFIG_SUNXI_SPIF_V2	0x10002

static int spif_select_buswidth(u32 buswidth)
{
	int width = 0;

	switch (buswidth) {
	case SPIF_SINGLE_MODE:
		width = 0;
		break;
	case SPIF_DUEL_MODE:
		width = 1;
		break;
	case SPIF_QUAD_MODE:
		width = 2;
		break;
	case SPIF_OCTAL_MODE:
		width = 3;
		break;
	default:
		printf("Parameter error with buswidth:%d\n", buswidth);
	}
	return width;
}

int spif_mem_exec_op_v0(struct spi_slave *slave, const struct spi_mem_op *op)
{
	int ret;
	struct spif_descriptor_op *spif_op = NULL;
	uint cache_buf[CONFIG_SYS_CACHELINE_SIZE] __aligned(CONFIG_SYS_CACHELINE_SIZE);
	uint desc_count = ((op->data.nbytes + SPIF_MAX_TRANS_NUM - 1) / SPIF_MAX_TRANS_NUM) + 1;
	uint desc_size = desc_count * sizeof(struct spif_descriptor_op);
	size_t remain_len = op->data.nbytes, handle_len = 0;

	spif_op = memalign(CONFIG_SYS_CACHELINE_SIZE, desc_size);

	memset(spif_op, 0, desc_size);
	memset(cache_buf, 0, CONFIG_SYS_CACHELINE_SIZE);

	/* set hburst type */
	spif_op->hburst_rw_flag &= ~HBURST_TYPE;
	spif_op->hburst_rw_flag |= HBURST_INCR4_TYPE;

	/* set DMA block len mode */
	spif_op->block_data_len &= ~DMA_BLK_LEN;
	spif_op->block_data_len |= DMA_BLK_LEN_16B;

	spif_op->addr_dummy_data_count |= SPIF_DES_NORMAL_EN;

	/* dispose cmd */
	if (op->cmd.opcode) {
		spif_op->trans_phase |= SPIF_CMD_TRANS_EN;
		spif_op->cmd_mode_buswidth |= op->cmd.opcode << SPIF_CMD_OPCODE_POS;
		/* set cmd buswidth */
		spif_op->cmd_mode_buswidth |=
			spif_select_buswidth(op->cmd.buswidth) << SPIF_CMD_TRANS_POS;
		if (op->cmd.buswidth != 1)
			spif_op->cmd_mode_buswidth |=
				spif_select_buswidth(op->cmd.buswidth) <<
				SPIF_DATA_TRANS_POS;
	}

	/* dispose addr */
	if (op->addr.nbytes) {
		spif_op->trans_phase |= SPIF_ADDR_TRANS_EN;
		spif_op->flash_addr = op->addr.val;
		if (op->addr.nbytes == 4) {//set 4byte addr mode
			spif_op->addr_dummy_data_count &= SPIF_ADDR_SIZE_MASK;
			spif_op->addr_dummy_data_count |= SPIF_ADDR_SIZE_32BIT;
		}
		/* set addr buswidth */
		spif_op->cmd_mode_buswidth |=
			spif_select_buswidth(op->addr.buswidth) <<
			SPIF_ADDR_TRANS_POS;
	}

	/* dispose mode */
	if (CHECK_IO_OP(op->cmd.opcode)) {
		spif_op->trans_phase |= SPIF_MODE_TRANS_EN;
		spif_op->cmd_mode_buswidth |=
				0x0 << SPIF_MODE_OPCODE_POS;
		/* set addr buswidth */
		spif_op->cmd_mode_buswidth |=
			spif_select_buswidth(op->addr.buswidth) <<
			SPIF_MODE_TRANS_POS;
	}

	/* dispose dummy */
	if (op->dummy.nbytes) {
		spif_op->trans_phase |= SPIF_DUMMY_TRANS_EN;
		spif_op->addr_dummy_data_count |=
			(op->dummy.nbytes << SPIF_DUMMY_NUM_POS);
	}

	/* dispose data */
	if (op->data.nbytes) {
		/* set data buswidth */
		spif_op->cmd_mode_buswidth |=
			spif_select_buswidth(op->data.buswidth) << SPIF_DATA_TRANS_POS;

		if (op->data.dir == SPI_MEM_DATA_IN) {
			spif_op->trans_phase |= SPIF_RX_TRANS_EN;
			if (op->data.nbytes < SPIF_MIN_TRANS_NUM)
				spif_op->data_addr = (u32)cache_buf;
			else
				spif_op->data_addr = (u32)op->data.buf.in;
			/* Write:1 DMA Write to dram */
			spif_op->hburst_rw_flag |= DMA_RW_PROCESS;
		} else {
			spif_op->trans_phase |= SPIF_TX_TRANS_EN;
			if (op->data.nbytes < NOR_PAGE_SIZE) {
				printf("Need to write by page\n");
				return -1;
			}
			spif_op->data_addr = (u32)op->data.buf.out;
			/* Read:0 DMA read for dram */
			spif_op->hburst_rw_flag &= ~DMA_RW_PROCESS;
		}
		if (op->data.nbytes < SPIF_MIN_TRANS_NUM &&
				op->data.dir == SPI_MEM_DATA_IN) {
			spif_op->addr_dummy_data_count |=
				SPIF_MIN_TRANS_NUM << SPIF_DATA_NUM_POS;
			spif_op->block_data_len |=
				SPIF_MIN_TRANS_NUM << SPIF_DATA_NUM_POS;

			spif_op->next_des_addr = 0;
			spif_op->hburst_rw_flag |= DMA_FINISH_FLASG;
		} else {
			struct spif_descriptor_op *current_op = spif_op;

			handle_len = min_t(size_t, SPIF_MAX_TRANS_NUM, remain_len);

			spif_op->addr_dummy_data_count |=
				handle_len << SPIF_DATA_NUM_POS;
			spif_op->block_data_len |=
				handle_len << SPIF_DATA_NUM_POS;

			remain_len = op->data.nbytes - handle_len;
			while (remain_len) {
				struct spif_descriptor_op *next_op = current_op + 1;

				memcpy(next_op, current_op, sizeof(struct spif_descriptor_op));

				next_op->data_addr +=  SPIF_MAX_TRANS_NUM;
				next_op->flash_addr += SPIF_MAX_TRANS_NUM;

				handle_len = min_t(size_t, SPIF_MAX_TRANS_NUM, remain_len);

				next_op->block_data_len &= ~DMA_DATA_LEN;
				next_op->block_data_len |=
					handle_len << SPIF_DATA_NUM_POS;

				next_op->addr_dummy_data_count &= ~DMA_TRANS_NUM;
				next_op->addr_dummy_data_count |=
					handle_len << SPIF_DATA_NUM_POS;

				/* set next des addr */
				current_op->next_des_addr = (unsigned int)next_op;

				remain_len -= handle_len;

				current_op = next_op;
			}
			current_op->next_des_addr = 0;
			current_op->hburst_rw_flag |= DMA_FINISH_FLASG;
		}

	}

	ret = spif_xfer(slave, spif_op, op->data.nbytes);

	if (op->data.nbytes < SPIF_MIN_TRANS_NUM &&
			op->data.dir == SPI_MEM_DATA_IN)
		memcpy((void *)op->data.buf.in,	(const void *)cache_buf,
					op->data.nbytes);
	if (spif_op)
		kfree(spif_op);
	return ret;
}

int spif_mem_exec_op_v1(struct spi_slave *slave, const struct spi_mem_op *op)
{
	int ret;
	struct spif_descriptor_op *spif_op = NULL;
	struct spif_descriptor_op *current_op = NULL;
	uint desc_count = ((op->data.nbytes + SPIF_MAX_TRANS_NUM - 1) / SPIF_MAX_TRANS_NUM) + 1;
	uint desc_size = desc_count * sizeof(struct spif_descriptor_op);
	size_t remain_len = op->data.nbytes, handle_len = 0;
	char *cache_buf = NULL;

	spif_op = memalign(CONFIG_SYS_CACHELINE_SIZE, desc_size);
	current_op = spif_op;

	memset(spif_op, 0, desc_size);

	/* set hburst type */
	spif_op->hburst_rw_flag &= ~HBURST_TYPE;
	spif_op->hburst_rw_flag |= HBURST_INCR16_TYPE;

	/* set DMA block len mode */
	spif_op->block_data_len &= ~DMA_BLK_LEN;
	spif_op->block_data_len |= DMA_BLK_LEN_64B;

	spif_op->addr_dummy_data_count |= SPIF_DES_NORMAL_EN;

	/* dispose cmd */
	if (op->cmd.opcode) {
		spif_op->trans_phase |= SPIF_CMD_TRANS_EN;
		spif_op->cmd_mode_buswidth |= op->cmd.opcode << SPIF_CMD_OPCODE_POS;
		/* set cmd buswidth */
		spif_op->cmd_mode_buswidth |=
			spif_select_buswidth(op->cmd.buswidth) << SPIF_CMD_TRANS_POS;
		if (op->cmd.buswidth != 1)
			spif_op->cmd_mode_buswidth |=
				spif_select_buswidth(op->cmd.buswidth) <<
				SPIF_DATA_TRANS_POS;
	}

	/* dispose addr */
	if (op->addr.nbytes) {
		spif_op->trans_phase |= SPIF_ADDR_TRANS_EN;
		spif_op->flash_addr = op->addr.val;
		if (op->addr.nbytes == 4) {//set 4byte addr mode
			if (sunxi_spif_get_version() >= CONFIG_SUNXI_SPIF_V2) {
				spif_op->addr_dummy_data_count &= ~SPIF_ADDR_SIZE_MASK_V2;
				spif_op->addr_dummy_data_count |= SPIF_ADDR_SIZE_32BIT_V2;
			} else {
				spif_op->addr_dummy_data_count &= ~SPIF_ADDR_SIZE_MASK;
				spif_op->addr_dummy_data_count |= SPIF_ADDR_SIZE_32BIT;
			}
		} else {
			if (sunxi_spif_get_version() >= CONFIG_SUNXI_SPIF_V2) {
				spif_op->addr_dummy_data_count &= ~SPIF_ADDR_SIZE_MASK_V2;
				spif_op->addr_dummy_data_count |= SPIF_ADDR_SIZE_24BIT_V2;
			} else {
				spif_op->addr_dummy_data_count &= ~SPIF_ADDR_SIZE_MASK;
				spif_op->addr_dummy_data_count |= SPIF_ADDR_SIZE_24BIT;
			}
		}
		/* set addr buswidth */
		spif_op->cmd_mode_buswidth |=
			spif_select_buswidth(op->addr.buswidth) <<
			SPIF_ADDR_TRANS_POS;
	}

	/* dispose mode */
	if (CHECK_IO_OP(op->cmd.opcode)) {
		spif_op->trans_phase |= SPIF_MODE_TRANS_EN;
		spif_op->cmd_mode_buswidth |=
				0x0 << SPIF_MODE_OPCODE_POS;
		/* set addr buswidth */
		spif_op->cmd_mode_buswidth |=
			spif_select_buswidth(op->addr.buswidth) <<
			SPIF_MODE_TRANS_POS;
	}

	/* dispose dummy */
	if (op->dummy.nbytes) {
		spif_op->trans_phase |= SPIF_DUMMY_TRANS_EN;
		spif_op->addr_dummy_data_count |=
			(op->dummy.nbytes << SPIF_DUMMY_NUM_POS);
	}

	/* dispose data */
	if (op->data.nbytes) {
		if (op->data.dir == SPI_MEM_DATA_IN) {
			spif_op->trans_phase |= SPIF_RX_TRANS_EN;
			if ((u32)op->data.buf.in % CONFIG_SYS_CACHELINE_SIZE) {
				cache_buf = memalign(CONFIG_SYS_CACHELINE_SIZE,
						op->data.nbytes);
				spif_op->data_addr = (u32)cache_buf;
			} else
				spif_op->data_addr = (u32)op->data.buf.in;
			/* Read Flash:1 DMA Write to dram */
			spif_op->hburst_rw_flag |= DMA_RW_PROCESS;
		} else {
			spif_op->trans_phase |= SPIF_TX_TRANS_EN;
			if ((u32)op->data.buf.out % CONFIG_SYS_CACHELINE_SIZE) {
				cache_buf = memalign(CONFIG_SYS_CACHELINE_SIZE,
						op->data.nbytes);
				memcpy(cache_buf, op->data.buf.out, op->data.nbytes);
				spif_op->data_addr = (u32)cache_buf;

			} else
				spif_op->data_addr = (u32)op->data.buf.out;
			/* Write Flash:0 DMA read for dram */
			spif_op->hburst_rw_flag &= ~DMA_RW_PROCESS;
		}

		/* addr word alignment */
		spif_op->data_addr = spif_op->data_addr >> 2;

		/* set data buswidth */
		spif_op->cmd_mode_buswidth |=
			spif_select_buswidth(op->data.buswidth) << SPIF_DATA_TRANS_POS;

		handle_len = min_t(size_t, SPIF_MAX_TRANS_NUM, remain_len);

		spif_op->block_data_len |=
			handle_len << SPIF_DATA_NUM_POS;
		spif_op->addr_dummy_data_count |= handle_len == SPIF_MAX_TRANS_NUM ?
			DMA_TRANS_NUM_16BIT : handle_len << SPIF_DATA_NUM_POS;

		remain_len = op->data.nbytes - handle_len;
		while (remain_len) {
			struct spif_descriptor_op *next_op = current_op + 1;

			memcpy(next_op, current_op, sizeof(struct spif_descriptor_op));

			next_op->flash_addr += SPIF_MAX_TRANS_NUM;
			next_op->data_addr += (SPIF_MAX_TRANS_NUM >> 2);

			handle_len = min_t(size_t, SPIF_MAX_TRANS_NUM, remain_len);

			next_op->block_data_len &= ~DMA_DATA_LEN;
			next_op->addr_dummy_data_count &=
				~(DMA_TRANS_NUM_16BIT | DMA_TRANS_NUM);

			next_op->block_data_len |=
				handle_len << SPIF_DATA_NUM_POS;
			next_op->addr_dummy_data_count |=
				handle_len == SPIF_MAX_TRANS_NUM ?
				DMA_TRANS_NUM_16BIT : (handle_len << SPIF_DATA_NUM_POS);

			/* set next des addr */
			current_op->next_des_addr = (u32)next_op >> 2;

			remain_len -= handle_len;

			current_op = next_op;
		}
		current_op->next_des_addr = 0;
		current_op->hburst_rw_flag |= DMA_FINISH_FLASG;
	}

	ret = spif_xfer(slave, spif_op, op->data.nbytes);

	if (op->data.dir == SPI_MEM_DATA_IN &&
			(u32)op->data.buf.in % CONFIG_SYS_CACHELINE_SIZE)
		memcpy((void *)op->data.buf.in,	(const void *)cache_buf,
					op->data.nbytes);

	if (spif_op)
		kfree(spif_op);
	if (cache_buf)
		kfree(cache_buf);

	return ret;
}
#endif /* CONFIG_AW_SPIF */




/**
 * spi_mem_exec_op() - Execute a memory operation
 * @slave: the SPI device
 * @op: the memory operation to execute
 *
 * Executes a memory operation.
 *
 * This function first checks that @op is supported and then tries to execute
 * it.
 *
 * Return: 0 in case of success, a negative error code otherwise.
 */

int spi_mem_exec_op(struct spi_slave *slave, const struct spi_mem_op *op)
{
	if (strcmp(slave->dev->driver->name, "spi_nand")) {
#ifdef CONFIG_AW_SPIF
	if (sunxi_spif_get_version() == CONFIG_SUNXI_SPIF_V0)
		return spif_mem_exec_op_v0(slave, op);
	else
		return spif_mem_exec_op_v1(slave, op);
#endif /* CONFIG_AW_SPIF */
	}

	struct udevice *bus = slave->dev->parent;
	struct dm_spi_ops *ops = spi_get_ops(bus);
	unsigned int pos = 0;
	const u8 *tx_buf = NULL;
	u8 *rx_buf = NULL;
	int op_len;
	u32 flag;
	int ret;
	int i;

	if (!spi_mem_supports_op(slave, op))
		return -ENOTSUPP;

	ret = spi_claim_bus(slave);
	if (ret < 0)
		return ret;

	if (ops->mem_ops && ops->mem_ops->exec_op) {
#ifndef __UBOOT__
		/*
		 * Flush the message queue before executing our SPI memory
		 * operation to prevent preemption of regular SPI transfers.
		 */
		spi_flush_queue(ctlr);

		if (ctlr->auto_runtime_pm) {
			ret = pm_runtime_get_sync(ctlr->dev.parent);
			if (ret < 0) {
				dev_err(&ctlr->dev,
					"Failed to power device: %d\n",
					ret);
				return ret;
			}
		}

		mutex_lock(&ctlr->bus_lock_mutex);
		mutex_lock(&ctlr->io_mutex);
#endif
		ret = ops->mem_ops->exec_op(slave, op);

#ifndef __UBOOT__
		mutex_unlock(&ctlr->io_mutex);
		mutex_unlock(&ctlr->bus_lock_mutex);

		if (ctlr->auto_runtime_pm)
			pm_runtime_put(ctlr->dev.parent);
#endif

		/*
		 * Some controllers only optimize specific paths (typically the
		 * read path) and expect the core to use the regular SPI
		 * interface in other cases.
		 */
		if (!ret || ret != -ENOTSUPP) {
			spi_release_bus(slave);
			return ret;
		}
	}

#ifndef __UBOOT__
	tmpbufsize = op->cmd.nbytes + op->addr.nbytes + op->dummy.nbytes;

	/*
	 * Allocate a buffer to transmit the CMD, ADDR cycles with kmalloc() so
	 * we're guaranteed that this buffer is DMA-able, as required by the
	 * SPI layer.
	 */
	tmpbuf = kzalloc(tmpbufsize, GFP_KERNEL | GFP_DMA);
	if (!tmpbuf)
		return -ENOMEM;

	spi_message_init(&msg);

	tmpbuf[0] = op->cmd.opcode;
	xfers[xferpos].tx_buf = tmpbuf;
	xfers[xferpos].len = op->cmd.nbytes;
	xfers[xferpos].tx_nbits = op->cmd.buswidth;
	spi_message_add_tail(&xfers[xferpos], &msg);
	xferpos++;
	totalxferlen++;

	if (op->addr.nbytes) {
		int i;

		for (i = 0; i < op->addr.nbytes; i++)
			tmpbuf[i + 1] = op->addr.val >>
					(8 * (op->addr.nbytes - i - 1));

		xfers[xferpos].tx_buf = tmpbuf + 1;
		xfers[xferpos].len = op->addr.nbytes;
		xfers[xferpos].tx_nbits = op->addr.buswidth;
		spi_message_add_tail(&xfers[xferpos], &msg);
		xferpos++;
		totalxferlen += op->addr.nbytes;
	}

	if (op->dummy.nbytes) {
		memset(tmpbuf + op->addr.nbytes + 1, 0xff, op->dummy.nbytes);
		xfers[xferpos].tx_buf = tmpbuf + op->addr.nbytes + 1;
		xfers[xferpos].len = op->dummy.nbytes;
		xfers[xferpos].tx_nbits = op->dummy.buswidth;
		spi_message_add_tail(&xfers[xferpos], &msg);
		xferpos++;
		totalxferlen += op->dummy.nbytes;
	}

	if (op->data.nbytes) {
		if (op->data.dir == SPI_MEM_DATA_IN) {
			xfers[xferpos].rx_buf = op->data.buf.in;
			xfers[xferpos].rx_nbits = op->data.buswidth;
		} else {
			xfers[xferpos].tx_buf = op->data.buf.out;
			xfers[xferpos].tx_nbits = op->data.buswidth;
		}

		xfers[xferpos].len = op->data.nbytes;
		spi_message_add_tail(&xfers[xferpos], &msg);
		xferpos++;
		totalxferlen += op->data.nbytes;
	}

	ret = spi_sync(slave, &msg);

	kfree(tmpbuf);

	if (ret)
		return ret;

	if (msg.actual_length != totalxferlen)
		return -EIO;
#else

	if (op->data.nbytes) {
		if (op->data.dir == SPI_MEM_DATA_IN)
			rx_buf = op->data.buf.in;
		else
			tx_buf = op->data.buf.out;
	}

	op_len = op->cmd.nbytes + op->addr.nbytes + op->dummy.nbytes;

	/*
	 * Avoid using malloc() here so that we can use this code in SPL where
	 * simple malloc may be used. That implementation does not allow free()
	 * so repeated calls to this code can exhaust the space.
	 *
	 * The value of op_len is small, since it does not include the actual
	 * data being sent, only the op-code and address. In fact, it should be
	 * possible to just use a small fixed value here instead of op_len.
	 */
	u8 op_buf[op_len];

	op_buf[pos++] = op->cmd.opcode;

	if (op->addr.nbytes) {
		for (i = 0; i < op->addr.nbytes; i++)
			op_buf[pos + i] = op->addr.val >>
				(8 * (op->addr.nbytes - i - 1));

		pos += op->addr.nbytes;
	}

	if (op->dummy.nbytes)
		memset(op_buf + pos, 0xff, op->dummy.nbytes);

	/* 1st transfer: opcode + address + dummy cycles */
	flag = SPI_XFER_BEGIN;
	/* Make sure to set END bit if no tx or rx data messages follow */
	if (!tx_buf && !rx_buf)
		flag |= SPI_XFER_END;

	ret = spi_xfer(slave, op_len * 8, op_buf, NULL, flag);
	if (ret)
		return ret;

	/* 2nd transfer: rx or tx data path */
	if (tx_buf || rx_buf) {
		ret = spi_xfer(slave, op->data.nbytes * 8, tx_buf,
			       rx_buf, SPI_XFER_END);
		if (ret)
			return ret;
	}

	spi_release_bus(slave);

	for (i = 0; i < pos; i++)
		debug("%02x ", op_buf[i]);
	debug("| [%dB %s] ",
	      tx_buf || rx_buf ? op->data.nbytes : 0,
	      tx_buf || rx_buf ? (tx_buf ? "out" : "in") : "-");
	for (i = 0; i < op->data.nbytes; i++)
		debug("%02x ", tx_buf ? tx_buf[i] : rx_buf[i]);
	debug("[ret %d]\n", ret);

	if (ret < 0)
		return ret;
#endif /* __UBOOT__ */

	return 0;
}
EXPORT_SYMBOL_GPL(spi_mem_exec_op);

/**
 * spi_mem_adjust_op_size() - Adjust the data size of a SPI mem operation to
 *				 match controller limitations
 * @slave: the SPI device
 * @op: the operation to adjust
 *
 * Some controllers have FIFO limitations and must split a data transfer
 * operation into multiple ones, others require a specific alignment for
 * optimized accesses. This function allows SPI mem drivers to split a single
 * operation into multiple sub-operations when required.
 *
 * Return: a negative error code if the controller can't properly adjust @op,
 *	   0 otherwise. Note that @op->data.nbytes will be updated if @op
 *	   can't be handled in a single step.
 */
int spi_mem_adjust_op_size(struct spi_slave *slave, struct spi_mem_op *op)
{
	struct udevice *bus = slave->dev->parent;
	struct dm_spi_ops *ops = spi_get_ops(bus);

	if (ops->mem_ops && ops->mem_ops->adjust_op_size)
		return ops->mem_ops->adjust_op_size(slave, op);

	if (!ops->mem_ops || !ops->mem_ops->exec_op) {
		unsigned int len;

		len = op->cmd.nbytes + op->addr.nbytes + op->dummy.nbytes;
		if (slave->max_write_size && len > slave->max_write_size)
			return -EINVAL;

		if (op->data.dir == SPI_MEM_DATA_IN) {
			if (slave->max_read_size)
				op->data.nbytes = min(op->data.nbytes,
					      slave->max_read_size);
		} else if (slave->max_write_size) {
			op->data.nbytes = min(op->data.nbytes,
					      slave->max_write_size - len);
		}

		if (!op->data.nbytes)
			return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(spi_mem_adjust_op_size);

static ssize_t spi_mem_no_dirmap_read(struct spi_mem_dirmap_desc *desc,
				      u64 offs, size_t len, void *buf)
{
	struct spi_mem_op op = desc->info.op_tmpl;
	int ret;

	op.addr.val = desc->info.offset + offs;
	op.data.buf.in = buf;
	op.data.nbytes = len;
	ret = spi_mem_adjust_op_size(desc->slave, &op);
	if (ret)
		return ret;

	ret = spi_mem_exec_op(desc->slave, &op);
	if (ret)
		return ret;

	return op.data.nbytes;
}

static ssize_t spi_mem_no_dirmap_write(struct spi_mem_dirmap_desc *desc,
				       u64 offs, size_t len, const void *buf)
{
	struct spi_mem_op op = desc->info.op_tmpl;
	int ret;

	op.addr.val = desc->info.offset + offs;
	op.data.buf.out = buf;
	op.data.nbytes = len;
	ret = spi_mem_adjust_op_size(desc->slave, &op);
	if (ret)
		return ret;

	ret = spi_mem_exec_op(desc->slave, &op);
	if (ret)
		return ret;

	return op.data.nbytes;
}

/**
 * spi_mem_dirmap_create() - Create a direct mapping descriptor
 * @mem: SPI mem device this direct mapping should be created for
 * @info: direct mapping information
 *
 * This function is creating a direct mapping descriptor which can then be used
 * to access the memory using spi_mem_dirmap_read() or spi_mem_dirmap_write().
 * If the SPI controller driver does not support direct mapping, this function
 * falls back to an implementation using spi_mem_exec_op(), so that the caller
 * doesn't have to bother implementing a fallback on his own.
 *
 * Return: a valid pointer in case of success, and ERR_PTR() otherwise.
 */
struct spi_mem_dirmap_desc *
spi_mem_dirmap_create(struct spi_slave *slave,
		      const struct spi_mem_dirmap_info *info)
{
	struct udevice *bus = slave->dev->parent;
	struct dm_spi_ops *ops = spi_get_ops(bus);
	struct spi_mem_dirmap_desc *desc;
	int ret = -EOPNOTSUPP;

	/* Make sure the number of address cycles is between 1 and 8 bytes. */
	if (!info->op_tmpl.addr.nbytes || info->op_tmpl.addr.nbytes > 8)
		return ERR_PTR(-EINVAL);

	/* data.dir should either be SPI_MEM_DATA_IN or SPI_MEM_DATA_OUT. */
	if (info->op_tmpl.data.dir == SPI_MEM_NO_DATA)
		return ERR_PTR(-EINVAL);

	desc = kzalloc(sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return ERR_PTR(-ENOMEM);

	desc->slave = slave;
	desc->info = *info;
	if (ops->mem_ops && ops->mem_ops->dirmap_create)
		ret = ops->mem_ops->dirmap_create(desc);

	if (ret) {
		desc->nodirmap = true;
		if (!spi_mem_supports_op(desc->slave, &desc->info.op_tmpl))
			ret = -EOPNOTSUPP;
		else
			ret = 0;
	}

	if (ret) {
		kfree(desc);
		return ERR_PTR(ret);
	}

	return desc;
}
EXPORT_SYMBOL_GPL(spi_mem_dirmap_create);

/**
 * spi_mem_dirmap_destroy() - Destroy a direct mapping descriptor
 * @desc: the direct mapping descriptor to destroy
 *
 * This function destroys a direct mapping descriptor previously created by
 * spi_mem_dirmap_create().
 */
void spi_mem_dirmap_destroy(struct spi_mem_dirmap_desc *desc)
{
	struct udevice *bus = desc->slave->dev->parent;
	struct dm_spi_ops *ops = spi_get_ops(bus);

	if (!desc->nodirmap && ops->mem_ops && ops->mem_ops->dirmap_destroy)
		ops->mem_ops->dirmap_destroy(desc);

	kfree(desc);
}
EXPORT_SYMBOL_GPL(spi_mem_dirmap_destroy);

#ifndef __UBOOT__
static void devm_spi_mem_dirmap_release(struct udevice *dev, void *res)
{
	struct spi_mem_dirmap_desc *desc = *(struct spi_mem_dirmap_desc **)res;

	spi_mem_dirmap_destroy(desc);
}

/**
 * devm_spi_mem_dirmap_create() - Create a direct mapping descriptor and attach
 *				  it to a device
 * @dev: device the dirmap desc will be attached to
 * @mem: SPI mem device this direct mapping should be created for
 * @info: direct mapping information
 *
 * devm_ variant of the spi_mem_dirmap_create() function. See
 * spi_mem_dirmap_create() for more details.
 *
 * Return: a valid pointer in case of success, and ERR_PTR() otherwise.
 */
struct spi_mem_dirmap_desc *
devm_spi_mem_dirmap_create(struct udevice *dev, struct spi_slave *slave,
			   const struct spi_mem_dirmap_info *info)
{
	struct spi_mem_dirmap_desc **ptr, *desc;

	ptr = devres_alloc(devm_spi_mem_dirmap_release, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return ERR_PTR(-ENOMEM);

	desc = spi_mem_dirmap_create(slave, info);
	if (IS_ERR(desc)) {
		devres_free(ptr);
	} else {
		*ptr = desc;
		devres_add(dev, ptr);
	}

	return desc;
}
EXPORT_SYMBOL_GPL(devm_spi_mem_dirmap_create);

static int devm_spi_mem_dirmap_match(struct udevice *dev, void *res, void *data)
{
	struct spi_mem_dirmap_desc **ptr = res;

	if (WARN_ON(!ptr || !*ptr))
		return 0;

	return *ptr == data;
}

/**
 * devm_spi_mem_dirmap_destroy() - Destroy a direct mapping descriptor attached
 *				   to a device
 * @dev: device the dirmap desc is attached to
 * @desc: the direct mapping descriptor to destroy
 *
 * devm_ variant of the spi_mem_dirmap_destroy() function. See
 * spi_mem_dirmap_destroy() for more details.
 */
void devm_spi_mem_dirmap_destroy(struct udevice *dev,
				 struct spi_mem_dirmap_desc *desc)
{
	devres_release(dev, devm_spi_mem_dirmap_release,
		       devm_spi_mem_dirmap_match, desc);
}
EXPORT_SYMBOL_GPL(devm_spi_mem_dirmap_destroy);
#endif /* __UBOOT__ */

/**
 * spi_mem_dirmap_read() - Read data through a direct mapping
 * @desc: direct mapping descriptor
 * @offs: offset to start reading from. Note that this is not an absolute
 *	  offset, but the offset within the direct mapping which already has
 *	  its own offset
 * @len: length in bytes
 * @buf: destination buffer. This buffer must be DMA-able
 *
 * This function reads data from a memory device using a direct mapping
 * previously instantiated with spi_mem_dirmap_create().
 *
 * Return: the amount of data read from the memory device or a negative error
 * code. Note that the returned size might be smaller than @len, and the caller
 * is responsible for calling spi_mem_dirmap_read() again when that happens.
 */
ssize_t spi_mem_dirmap_read(struct spi_mem_dirmap_desc *desc,
			    u64 offs, size_t len, void *buf)
{
	struct udevice *bus = desc->slave->dev->parent;
	struct dm_spi_ops *ops = spi_get_ops(bus);
	ssize_t ret;

	if (desc->info.op_tmpl.data.dir != SPI_MEM_DATA_IN)
		return -EINVAL;

	if (!len)
		return 0;

	if (desc->nodirmap)
		ret = spi_mem_no_dirmap_read(desc, offs, len, buf);
	else if (ops->mem_ops && ops->mem_ops->dirmap_read)
		ret = ops->mem_ops->dirmap_read(desc, offs, len, buf);
	else
		ret = -EOPNOTSUPP;

	return ret;
}
EXPORT_SYMBOL_GPL(spi_mem_dirmap_read);

/**
 * spi_mem_dirmap_write() - Write data through a direct mapping
 * @desc: direct mapping descriptor
 * @offs: offset to start writing from. Note that this is not an absolute
 *	  offset, but the offset within the direct mapping which already has
 *	  its own offset
 * @len: length in bytes
 * @buf: source buffer. This buffer must be DMA-able
 *
 * This function writes data to a memory device using a direct mapping
 * previously instantiated with spi_mem_dirmap_create().
 *
 * Return: the amount of data written to the memory device or a negative error
 * code. Note that the returned size might be smaller than @len, and the caller
 * is responsible for calling spi_mem_dirmap_write() again when that happens.
 */
ssize_t spi_mem_dirmap_write(struct spi_mem_dirmap_desc *desc,
			     u64 offs, size_t len, const void *buf)
{
	struct udevice *bus = desc->slave->dev->parent;
	struct dm_spi_ops *ops = spi_get_ops(bus);
	ssize_t ret;

	if (desc->info.op_tmpl.data.dir != SPI_MEM_DATA_OUT)
		return -EINVAL;

	if (!len)
		return 0;

	if (desc->nodirmap)
		ret = spi_mem_no_dirmap_write(desc, offs, len, buf);
	else if (ops->mem_ops && ops->mem_ops->dirmap_write)
		ret = ops->mem_ops->dirmap_write(desc, offs, len, buf);
	else
		ret = -EOPNOTSUPP;

	return ret;
}
EXPORT_SYMBOL_GPL(spi_mem_dirmap_write);

#ifndef __UBOOT__
static inline struct spi_mem_driver *to_spi_mem_drv(struct device_driver *drv)
{
	return container_of(drv, struct spi_mem_driver, spidrv.driver);
}

static int spi_mem_probe(struct spi_device *spi)
{
	struct spi_mem_driver *memdrv = to_spi_mem_drv(spi->dev.driver);
	struct spi_mem *mem;

	mem = devm_kzalloc(&spi->dev, sizeof(*mem), GFP_KERNEL);
	if (!mem)
		return -ENOMEM;

	mem->spi = spi;
	spi_set_drvdata(spi, mem);

	return memdrv->probe(mem);
}

static int spi_mem_remove(struct spi_device *spi)
{
	struct spi_mem_driver *memdrv = to_spi_mem_drv(spi->dev.driver);
	struct spi_mem *mem = spi_get_drvdata(spi);

	if (memdrv->remove)
		return memdrv->remove(mem);

	return 0;
}

static void spi_mem_shutdown(struct spi_device *spi)
{
	struct spi_mem_driver *memdrv = to_spi_mem_drv(spi->dev.driver);
	struct spi_mem *mem = spi_get_drvdata(spi);

	if (memdrv->shutdown)
		memdrv->shutdown(mem);
}

/**
 * spi_mem_driver_register_with_owner() - Register a SPI memory driver
 * @memdrv: the SPI memory driver to register
 * @owner: the owner of this driver
 *
 * Registers a SPI memory driver.
 *
 * Return: 0 in case of success, a negative error core otherwise.
 */

int spi_mem_driver_register_with_owner(struct spi_mem_driver *memdrv,
				       struct module *owner)
{
	memdrv->spidrv.probe = spi_mem_probe;
	memdrv->spidrv.remove = spi_mem_remove;
	memdrv->spidrv.shutdown = spi_mem_shutdown;

	return __spi_register_driver(owner, &memdrv->spidrv);
}
EXPORT_SYMBOL_GPL(spi_mem_driver_register_with_owner);

/**
 * spi_mem_driver_unregister_with_owner() - Unregister a SPI memory driver
 * @memdrv: the SPI memory driver to unregister
 *
 * Unregisters a SPI memory driver.
 */
void spi_mem_driver_unregister(struct spi_mem_driver *memdrv)
{
	spi_unregister_driver(&memdrv->spidrv);
}
EXPORT_SYMBOL_GPL(spi_mem_driver_unregister);
#endif /* __UBOOT__ */
