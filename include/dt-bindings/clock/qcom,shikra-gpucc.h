/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_GPU_CC_SHIKRA_H
#define _DT_BINDINGS_CLK_QCOM_GPU_CC_SHIKRA_H

/* GPU_CC clocks */
#define GPU_CC_PLL0						0
#define GPU_CC_AHB_CLK						1
#define GPU_CC_CRC_AHB_CLK					2
#define GPU_CC_CX_GFX3D_CLK					3
#define GPU_CC_CX_GFX3D_SLV_CLK					4
#define GPU_CC_CX_GMU_CLK					5
#define GPU_CC_CX_SNOC_DVM_CLK					6
#define GPU_CC_CXO_AON_CLK					7
#define GPU_CC_CXO_CLK						8
#define GPU_CC_GMU_CLK_SRC					9
#define GPU_CC_GPU_SMMU_VOTE_CLK				10
#define GPU_CC_GX_CXO_CLK					11
#define GPU_CC_GX_GFX3D_CLK					12
#define GPU_CC_GX_GFX3D_CLK_SRC					13
#define GPU_CC_SLEEP_CLK					14

/* GPU_CC power domains */
#define GPU_CC_CX_GDSC						0
#define GPU_CC_GX_GDSC						1

/* GPU_CC resets */
#define GPU_CC_CX_BCR						0
#define GPU_CC_GFX3D_AON_BCR					1
#define GPU_CC_GMU_BCR						2
#define GPU_CC_GX_BCR						3
#define GPU_CC_XO_BCR						4

#endif
