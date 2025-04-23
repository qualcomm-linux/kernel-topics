/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _CORESIGHT_TGU_H
#define _CORESIGHT_TGU_H

/* Register addresses */
#define TGU_CONTROL 0x0000

/* Register read/write */
#define tgu_writel(drvdata, val, off) __raw_writel((val), drvdata->base + off)
#define tgu_readl(drvdata, off) __raw_readl(drvdata->base + off)

/**
 * struct tgu_drvdata - Data structure for a TGU (Trigger Generator Unit)
 * @base: Memory-mapped base address of the TGU device
 * @dev: Pointer to the associated device structure
 * @csdev: Pointer to the associated coresight device
 * @spinlock: Spinlock for handling concurrent access
 * @enable: Flag indicating whether the TGU device is enabled
 *
 * This structure defines the data associated with a TGU device,
 * including its base address, device pointers, clock, spinlock for
 * synchronization, trigger data pointers, maximum limits for various
 * trigger-related parameters, and enable status.
 */
struct tgu_drvdata {
	void __iomem *base;
	struct device *dev;
	struct coresight_device *csdev;
	spinlock_t spinlock;
	bool enable;
};

#endif
