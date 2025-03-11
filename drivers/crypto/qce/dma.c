// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <crypto/scatterwalk.h>

#include "common.h"
#include "core.h"
#include "dma.h"

struct qce_bam_transaction {
	struct bam_cmd_element qce_bam_ce[QCE_BAM_CMD_ELEMENT_SIZE];
	struct scatterlist qce_reg_write_sgl[QCE_BAM_CMD_SGL_SIZE];
	struct qce_desc_info *qce_desc;
	u32 qce_bam_ce_index;
	u32 qce_pre_bam_ce_index;
	u32 qce_write_sgl_cnt;
};

void qce_clear_bam_transaction(struct qce_device *qce)
{
	struct qce_bam_transaction *qce_bam_txn = qce->dma.qce_bam_txn;

	memset(&qce_bam_txn->qce_bam_ce_index, 0, sizeof(u32) * 8);
}

static int qce_dma_prep_cmd_sg(struct qce_device *qce, struct dma_chan *chan,
			       struct scatterlist *qce_bam_sgl,
			       int qce_sgl_cnt, unsigned long flags,
			       enum dma_transfer_direction dir_eng,
			       dma_async_tx_callback cb, void *cb_param)
{
	struct dma_async_tx_descriptor *dma_desc;
	struct qce_desc_info *desc;
	dma_cookie_t cookie;

	desc = qce->dma.qce_bam_txn->qce_desc;

	if (dir_eng == DMA_MEM_TO_DEV)
		desc->dir = DMA_TO_DEVICE;
	if (dir_eng == DMA_DEV_TO_MEM)
		desc->dir = DMA_FROM_DEVICE;

	if (!qce_bam_sgl || !qce_sgl_cnt)
		return -EINVAL;

	if (!dma_map_sg(qce->dev, qce_bam_sgl,
			qce_sgl_cnt, desc->dir)) {
		dev_err(qce->dev, "failure in mapping sgl for cmd desc\n");
		return -ENOMEM;
	}

	dma_desc = dmaengine_prep_slave_sg(chan, qce_bam_sgl, qce_sgl_cnt,
					   dir_eng, flags);
	if (!dma_desc) {
		dev_err(qce->dev, "failed to prepare the command descriptor\n");
		dma_unmap_sg(qce->dev, qce_bam_sgl, qce_sgl_cnt, desc->dir);
		kfree(desc);
		return -EINVAL;
	}

	desc->dma_desc = dma_desc;
	desc->dma_desc->callback = cb;
	desc->dma_desc->callback_param = cb_param;

	cookie = dmaengine_submit(desc->dma_desc);

	return dma_submit_error(cookie);
}

int qce_submit_cmd_desc(struct qce_device *qce, unsigned long flags)
{
	struct qce_bam_transaction *qce_bam_txn = qce->dma.qce_bam_txn;
	struct dma_chan *chan = qce->dma.rxchan;
	unsigned long desc_flags;
	int ret = 0;

	desc_flags = DMA_PREP_CMD;
	if (flags & QCE_DMA_DESC_FLAG_LOCK)
		desc_flags |= DMA_PREP_LOCK;
	else if (flags & QCE_DMA_DESC_FLAG_UNLOCK)
		desc_flags |= DMA_PREP_UNLOCK;

	/*
	 * The HPG recommends always using the consumer pipe for command
	 * descriptors.
	 */
	if (qce_bam_txn->qce_write_sgl_cnt)
		ret = qce_dma_prep_cmd_sg(qce, chan, qce_bam_txn->qce_reg_write_sgl,
					  qce_bam_txn->qce_write_sgl_cnt,
					  desc_flags, DMA_MEM_TO_DEV,
					  NULL, NULL);
	if (ret) {
		dev_err(qce->dev,
			"error while submitting the command descriptor for TX: %d\n",
			ret);
		return ret;
	}

	qce_dma_issue_pending(&qce->dma);

	if (qce_bam_txn->qce_write_sgl_cnt)
		dma_unmap_sg(qce->dev, qce_bam_txn->qce_reg_write_sgl,
			     qce_bam_txn->qce_write_sgl_cnt,
			     DMA_TO_DEVICE);

	return ret;
}

static void qce_prep_dma_command_desc(struct qce_device *qce,
				      struct qce_dma_data *dma,
				      unsigned int addr, void *buff)
{
	struct qce_bam_transaction *qce_bam_txn = dma->qce_bam_txn;
	struct bam_cmd_element *qce_bam_ce_buffer;
	int qce_bam_ce_size, cnt, index;

	index = qce_bam_txn->qce_bam_ce_index;
	qce_bam_ce_buffer = &qce_bam_txn->qce_bam_ce[index];
	bam_prep_ce_le32(qce_bam_ce_buffer, addr, BAM_WRITE_COMMAND,
			 *((__le32 *)buff));

	cnt = qce_bam_txn->qce_write_sgl_cnt;
	qce_bam_ce_buffer =
		&qce_bam_txn->qce_bam_ce[qce_bam_txn->qce_pre_bam_ce_index];
	++qce_bam_txn->qce_bam_ce_index;
	qce_bam_ce_size = (qce_bam_txn->qce_bam_ce_index -
			   qce_bam_txn->qce_pre_bam_ce_index) *
			  sizeof(struct bam_cmd_element);

	sg_set_buf(&qce_bam_txn->qce_reg_write_sgl[cnt], qce_bam_ce_buffer,
		   qce_bam_ce_size);

	++qce_bam_txn->qce_write_sgl_cnt;
	qce_bam_txn->qce_pre_bam_ce_index = qce_bam_txn->qce_bam_ce_index;
}

void qce_write(struct qce_device *qce, unsigned int offset, u32 val)
{
	qce_prep_dma_command_desc(qce, &qce->dma, (qce->base_dma + offset),
				  &val);
}

static void qce_dma_release(void *data)
{
	struct qce_dma_data *dma = data;

	dma_release_channel(dma->txchan);
	dma_release_channel(dma->rxchan);
}

int devm_qce_dma_request(struct device *dev, struct qce_dma_data *dma)
{
	struct qce_bam_transaction *qce_bam_txn;
	int ret;

	dma->txchan = dma_request_chan(dev, "tx");
	if (IS_ERR(dma->txchan))
		return PTR_ERR(dma->txchan);

	dma->rxchan = dma_request_chan(dev, "rx");
	if (IS_ERR(dma->rxchan)) {
		dma_release_channel(dma->txchan);
		return PTR_ERR(dma->rxchan);
	}

	ret = devm_add_action_or_reset(dev, qce_dma_release, dma);
	if (ret)
		return ret;

	dma->result_buf = devm_kmalloc(dev,
				       QCE_RESULT_BUF_SZ + QCE_IGNORE_BUF_SZ,
				       GFP_KERNEL);
	if (!dma->result_buf)
		return -ENOMEM;

	dma->ignore_buf = dma->result_buf + QCE_RESULT_BUF_SZ;

	dma->qce_bam_txn = devm_kmalloc(dev, sizeof(*qce_bam_txn), GFP_KERNEL);
	if (!dma->qce_bam_txn)
		return -ENOMEM;

	dma->qce_bam_txn->qce_desc = devm_kzalloc(dev,
					sizeof(*dma->qce_bam_txn->qce_desc),
					GFP_KERNEL);
	if (!dma->qce_bam_txn->qce_desc)
		return -ENOMEM;

	sg_init_table(dma->qce_bam_txn->qce_reg_write_sgl,
		      QCE_BAM_CMD_SGL_SIZE);

	return 0;
}

struct scatterlist *
qce_sgtable_add(struct sg_table *sgt, struct scatterlist *new_sgl,
		unsigned int max_len)
{
	struct scatterlist *sg = sgt->sgl, *sg_last = NULL;
	unsigned int new_len;

	while (sg) {
		if (!sg_page(sg))
			break;
		sg = sg_next(sg);
	}

	if (!sg)
		return ERR_PTR(-EINVAL);

	while (new_sgl && sg && max_len) {
		new_len = new_sgl->length > max_len ? max_len : new_sgl->length;
		sg_set_page(sg, sg_page(new_sgl), new_len, new_sgl->offset);
		sg_last = sg;
		sg = sg_next(sg);
		new_sgl = sg_next(new_sgl);
		max_len -= new_len;
	}

	return sg_last;
}

static int qce_dma_prep_sg(struct dma_chan *chan, struct scatterlist *sg,
			   int nents, unsigned long flags,
			   enum dma_transfer_direction dir,
			   dma_async_tx_callback cb, void *cb_param)
{
	struct dma_async_tx_descriptor *desc;
	dma_cookie_t cookie;

	if (!sg || !nents)
		return -EINVAL;

	desc = dmaengine_prep_slave_sg(chan, sg, nents, dir, flags);
	if (!desc)
		return -EINVAL;

	desc->callback = cb;
	desc->callback_param = cb_param;
	cookie = dmaengine_submit(desc);

	return dma_submit_error(cookie);
}

int qce_dma_prep_sgs(struct qce_dma_data *dma, struct scatterlist *rx_sg,
		     int rx_nents, struct scatterlist *tx_sg, int tx_nents,
		     dma_async_tx_callback cb, void *cb_param)
{
	struct dma_chan *rxchan = dma->rxchan;
	struct dma_chan *txchan = dma->txchan;
	unsigned long flags = DMA_PREP_INTERRUPT | DMA_CTRL_ACK;
	int ret;

	ret = qce_dma_prep_sg(rxchan, rx_sg, rx_nents, flags, DMA_MEM_TO_DEV,
			     NULL, NULL);
	if (ret)
		return ret;

	return qce_dma_prep_sg(txchan, tx_sg, tx_nents, flags, DMA_DEV_TO_MEM,
			       cb, cb_param);
}

void qce_dma_issue_pending(struct qce_dma_data *dma)
{
	dma_async_issue_pending(dma->rxchan);
	dma_async_issue_pending(dma->txchan);
}

int qce_dma_terminate_all(struct qce_dma_data *dma)
{
	int ret;

	ret = dmaengine_terminate_all(dma->rxchan);
	return ret ?: dmaengine_terminate_all(dma->txchan);
}
