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

#define TGU_DEVID_SENSE_INPUT(devid_val) ((int) BMVAL(devid_val, 10, 17))
#define TGU_DEVID_STEPS(devid_val) ((int)BMVAL(devid_val, 3, 6))
#define NUMBER_BITS_EACH_SIGNAL 4
#define LENGTH_REGISTER 32

/*
 *  TGU configuration space                              Step configuration
 *  offset table                                         space layout
 * x-------------------------x$                          x-------------x$
 * |                         |$                          |             |$
 * |                         |                           |   reserve   |$
 * |                         |                           |             |$
 * |coresight management     |                           |-------------|base+n*0x1D8+0x1F4$
 * |     registe             |                     |---> |prioroty[3]  |$
 * |                         |                     |     |-------------|base+n*0x1D8+0x194$
 * |                         |                     |     |prioroty[2]  |$
 * |-------------------------|                     |     |-------------|base+n*0x1D8+0x134$
 * |                         |                     |     |prioroty[1]  |$
 * |         step[7]         |                     |     |-------------|base+n*0x1D8+0xD4$
 * |-------------------------|->base+0x40+7*0x1D8  |     |prioroty[0]  |$
 * |                         |                     |     |-------------|base+n*0x1D8+0x74$
 * |         ...             |                     |     |  condition  |$
 * |                         |                     |     |   select    |$
 * |-------------------------|->base+0x40+1*0x1D8  |     |-------------|base+n*0x1D8+0x60$
 * |                         |                     |     |  condition  |$
 * |         step[0]         |-------------------->      |   decode    |$
 * |-------------------------|-> base+0x40               |-------------|base+n*0x1D8+0x50$
 * |                         |                           |             |$
 * | Control and status space|                           |Timer/Counter|$
 * |        space            |                           |             |$
 * x-------------------------x->base                     x-------------x base+n*0x1D8+0x40$
 *
 */
#define STEP_OFFSET 0x1D8
#define PRIORITY_START_OFFSET 0x0074
#define PRIORITY_OFFSET 0x60
#define REG_OFFSET 0x4

/* Calculate compare step addresses */
#define PRIORITY_REG_STEP(step, priority, reg)\
	(PRIORITY_START_OFFSET + PRIORITY_OFFSET * priority +\
	REG_OFFSET * reg + STEP_OFFSET * step)

#define tgu_dataset_rw(name, step_index, type, reg_num)                  \
	(&((struct tgu_attribute[]){ {                                   \
		__ATTR(name, 0644, tgu_dataset_show, tgu_dataset_store), \
		step_index,                                              \
		type,                                                    \
		reg_num,                                                 \
	} })[0].attr.attr)

#define STEP_PRIORITY(step_index, reg_num, priority)                     \
	tgu_dataset_rw(reg##reg_num, step_index, TGU_PRIORITY##priority, \
		       reg_num)

#define STEP_PRIORITY_LIST(step_index, priority)  \
	{STEP_PRIORITY(step_index, 0, priority),  \
	 STEP_PRIORITY(step_index, 1, priority),  \
	 STEP_PRIORITY(step_index, 2, priority),  \
	 STEP_PRIORITY(step_index, 3, priority),  \
	 STEP_PRIORITY(step_index, 4, priority),  \
	 STEP_PRIORITY(step_index, 5, priority),  \
	 STEP_PRIORITY(step_index, 6, priority),  \
	 STEP_PRIORITY(step_index, 7, priority),  \
	 STEP_PRIORITY(step_index, 8, priority),  \
	 STEP_PRIORITY(step_index, 9, priority),  \
	 STEP_PRIORITY(step_index, 10, priority), \
	 STEP_PRIORITY(step_index, 11, priority), \
	 STEP_PRIORITY(step_index, 12, priority), \
	 STEP_PRIORITY(step_index, 13, priority), \
	 STEP_PRIORITY(step_index, 14, priority), \
	 STEP_PRIORITY(step_index, 15, priority), \
	 STEP_PRIORITY(step_index, 16, priority), \
	 STEP_PRIORITY(step_index, 17, priority), \
	 NULL			\
	}

#define PRIORITY_ATTRIBUTE_GROUP_INIT(step, priority)\
	(&(const struct attribute_group){\
		.attrs = (struct attribute*[])STEP_PRIORITY_LIST(step, priority),\
		.is_visible = tgu_node_visible,\
		.name = "step" #step "_priority" #priority \
	})

enum operation_index {
	TGU_PRIORITY0,
	TGU_PRIORITY1,
	TGU_PRIORITY2,
	TGU_PRIORITY3

};

/* Maximum priority that TGU supports */
#define MAX_PRIORITY 4

struct tgu_attribute {
	struct device_attribute attr;
	u32 step_index;
	enum operation_index operation_index;
	u32 reg_num;
};

struct value_table {
	unsigned int *priority;
};

/**
 * struct tgu_drvdata - Data structure for a TGU (Trigger Generator Unit)
 * @base: Memory-mapped base address of the TGU device
 * @dev: Pointer to the associated device structure
 * @csdev: Pointer to the associated coresight device
 * @spinlock: Spinlock for handling concurrent access
 * @enable: Flag indicating whether the TGU device is enabled
 * @value_table: Store given value based on relevant parameters.
 * @max_reg: Maximum number of registers
 * @max_step: Maximum step size
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
	struct value_table *value_table;
	int max_reg;
	int max_step;
};

#endif
