/* SPDX-License-Identifier: GPL-2.0 */
/*
 * ARM Error Source Table Support
 *
 * Copyright (c) 2025, Alibaba Group.
 */

#include <linux/acpi_aest.h>
#include <asm/ras.h>

#define MAX_GSI_PER_NODE 2

#define aest_dev_err(__adev, format, ...) \
	dev_err((__adev)->dev, format, ##__VA_ARGS__)
#define aest_dev_info(__adev, format, ...) \
	dev_info((__adev)->dev, format, ##__VA_ARGS__)
#define aest_dev_dbg(__adev, format, ...) \
	dev_dbg((__adev)->dev, format, ##__VA_ARGS__)

#define aest_node_err(__node, format, ...)                          \
	dev_err((__node)->adev->dev, "%s: " format, (__node)->name, \
		##__VA_ARGS__)
#define aest_node_info(__node, format, ...)                          \
	dev_info((__node)->adev->dev, "%s: " format, (__node)->name, \
		 ##__VA_ARGS__)
#define aest_node_dbg(__node, format, ...)                          \
	dev_dbg((__node)->adev->dev, "%s: " format, (__node)->name, \
		##__VA_ARGS__)

#define aest_record_err(__record, format, ...)                  \
	dev_err((__record)->node->adev->dev, "%s: %s: " format, \
		(__record)->node->name, (__record)->name, ##__VA_ARGS__)
#define aest_record_info(__record, format, ...)                  \
	dev_info((__record)->node->adev->dev, "%s: %s: " format, \
		 (__record)->node->name, (__record)->name, ##__VA_ARGS__)
#define aest_record_dbg(__record, format, ...)                  \
	dev_dbg((__record)->node->adev->dev, "%s: %s: " format, \
		(__record)->node->name, (__record)->name, ##__VA_ARGS__)

#define ERXGROUP_4K_OFFSET 0xE00
#define ERXGROUP_16K_OFFSET 0x3800
#define ERXGROUP_64K_OFFSET 0xE000
#define ERXGROUP_4K_SIZE (4 * KB)
#define ERXGROUP_16K_SIZE (16 * KB)
#define ERXGROUP_64K_SIZE (64 * KB)
#define ERXGROUP_4K_ERRGSR_NUM 1
#define ERXGROUP_16K_ERRGSR_NUM 4
#define ERXGROUP_64K_ERRGSR_NUM 14

struct aest_record {
	char *name;
	int index;
	void __iomem *regs_base;

	/*
	 * This bit specifies the addressing mode  to populate the ERR_ADDR
	 * register:
	 *   0b: Error record reports System Physical Addresses (SPA) in
	 *       the ERR_ADDR register.
	 *   1b: Error record reports error node-specific Logical Addresses(LA)
	 *       in the ERR_ADD register. OS must use other means to translate
	 *       the reported LA into SPA
	 */
	int addressing_mode;
	struct aest_node *node;
};

struct aest_group {
	int type;
	int errgsr_num;
	size_t size;
	u64 errgsr_offset;
};

static const struct aest_group aest_group_config[] = {
	[ACPI_AEST_NODE_GROUP_FORMAT_4K] = {
		.type = ACPI_AEST_NODE_GROUP_FORMAT_4K,
		.errgsr_num = ERXGROUP_4K_ERRGSR_NUM,
		.size = ERXGROUP_4K_SIZE,
		.errgsr_offset = ERXGROUP_4K_OFFSET,
	},
	[ACPI_AEST_NODE_GROUP_FORMAT_16K] = {
		.type = ACPI_AEST_NODE_GROUP_FORMAT_16K,
		.errgsr_num = ERXGROUP_16K_ERRGSR_NUM,
		.size = ERXGROUP_16K_SIZE,
		.errgsr_offset = ERXGROUP_16K_OFFSET,
	},
	[ACPI_AEST_NODE_GROUP_FORMAT_64K] = {
		.type = ACPI_AEST_NODE_GROUP_FORMAT_64K,
		.errgsr_num = ERXGROUP_64K_ERRGSR_NUM,
		.size = ERXGROUP_64K_SIZE,
		.errgsr_offset = ERXGROUP_64K_OFFSET,
	},
};

struct aest_node {
	char *name;
	u8 type;
	void *errgsr;
	void *base;

	/*
	 * This bitmap indicates which of the error records within this error
	 * node must be polled for error status.
	 * Bit[n] of this field pertains to error record corresponding to
	 * index n in this error group.
	 * Bit[n] = 0b: Error record at index n needs to be polled.
	 * Bit[n] = 1b: Error record at index n do not needs to be polled.
	 */
	unsigned long *record_implemented;
	/*
	 * This bitmap indicates which of the error records within this error
	 * node support error status reporting using ERRGSR register.
	 * Bit[n] of this field pertains to error record corresponding to
	 * index n in this error group.
	 * Bit[n] = 0b: Error record at index n supports error status reporting
	 *              through ERRGSR.S.
	 * Bit[n] = 1b: Error record at index n does not support error reporting
	 *              through the ERRGSR.S bit If this error record is
	 *              implemented, then it must be polled explicitly for
	 *              error events.
	 */
	unsigned long *status_reporting;

	const struct aest_group *group;
	struct aest_device *adev;
	struct acpi_aest_node *info;

	int record_count;
	struct aest_record *records;
};

struct aest_device {
	struct device *dev;
	u32 type;
	int node_cnt;
	struct aest_node *nodes;
	u32 id;
};

static const char *const aest_node_name[] = {
	[ACPI_AEST_PROCESSOR_ERROR_NODE] = "processor",
	[ACPI_AEST_MEMORY_ERROR_NODE] = "memory",
	[ACPI_AEST_SMMU_ERROR_NODE] = "smmu",
	[ACPI_AEST_VENDOR_ERROR_NODE] = "vendor",
	[ACPI_AEST_GIC_ERROR_NODE] = "gic",
	[ACPI_AEST_PCIE_ERROR_NODE] = "pcie",
	[ACPI_AEST_PROXY_ERROR_NODE] = "proxy",
};

static inline int aest_set_name(struct aest_device *adev,
				struct aest_hnode *ahnode)
{
	adev->dev->init_name = devm_kasprintf(adev->dev, GFP_KERNEL, "%s%d",
					      aest_node_name[ahnode->type],
					      adev->id);
	if (!adev->dev->init_name)
		return -ENOMEM;

	return 0;
}
