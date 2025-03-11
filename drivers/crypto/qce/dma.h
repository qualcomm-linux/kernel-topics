/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2011-2014, The Linux Foundation. All rights reserved.
 */

#ifndef _DMA_H_
#define _DMA_H_

#include <linux/dma/qcom_bam_dma.h>
#include <linux/dmaengine.h>

struct qce_device;

/* maximum data transfer block size between BAM and CE */
#define QCE_BAM_BURST_SIZE		64

#define QCE_AUTHIV_REGS_CNT		16
#define QCE_AUTH_BYTECOUNT_REGS_CNT	4
#define QCE_CNTRIV_REGS_CNT		4
#define QCE_BAM_CMD_SGL_SIZE           64
#define QCE_BAM_CMD_ELEMENT_SIZE       64
#define QCE_DMA_DESC_FLAG_BAM_NWD      (0x0004)
#define QCE_MAX_REG_READ               8
#define QCE_DMA_DESC_FLAG_LOCK          (0x0002)
#define QCE_DMA_DESC_FLAG_UNLOCK        (0x0001)


struct qce_result_dump {
	u32 auth_iv[QCE_AUTHIV_REGS_CNT];
	u32 auth_byte_count[QCE_AUTH_BYTECOUNT_REGS_CNT];
	u32 encr_cntr_iv[QCE_CNTRIV_REGS_CNT];
	u32 status;
	u32 status2;
};

#define QCE_IGNORE_BUF_SZ	(2 * QCE_BAM_BURST_SIZE)
#define QCE_RESULT_BUF_SZ	\
		ALIGN(sizeof(struct qce_result_dump), QCE_BAM_BURST_SIZE)

struct qce_dma_data {
	struct dma_chan *txchan;
	struct dma_chan *rxchan;
	struct qce_result_dump *result_buf;
	struct qce_bam_transaction *qce_bam_txn;
	void *ignore_buf;
};

struct qce_desc_info {
	struct dma_async_tx_descriptor *dma_desc;
	enum dma_data_direction dir;
};

int devm_qce_dma_request(struct device *dev, struct qce_dma_data *dma);
int qce_dma_prep_sgs(struct qce_dma_data *dma, struct scatterlist *sg_in,
		     int in_ents, struct scatterlist *sg_out, int out_ents,
		     dma_async_tx_callback cb, void *cb_param);
void qce_dma_issue_pending(struct qce_dma_data *dma);
int qce_dma_terminate_all(struct qce_dma_data *dma);
struct scatterlist *
qce_sgtable_add(struct sg_table *sgt, struct scatterlist *sg_add,
		unsigned int max_len);

void qce_clear_bam_transaction(struct qce_device *qce);
int qce_submit_cmd_desc(struct qce_device *qce, unsigned long flags);

#endif /* _DMA_H_ */
