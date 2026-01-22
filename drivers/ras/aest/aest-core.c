// SPDX-License-Identifier: GPL-2.0
/*
 * ARM Error Source Table Support
 *
 * Copyright (c) 2025, Alibaba Group.
 */

#include <linux/platform_device.h>
#include <linux/xarray.h>
#include <linux/ras.h>

#include "aest.h"

DEFINE_PER_CPU(struct aest_device, percpu_adev);

#undef pr_fmt
#define pr_fmt(fmt) "AEST: " fmt

static int get_aest_node_ver(struct aest_node *node)
{
	u64 reg;
	void *devarch_base;

	if (node->type == ACPI_AEST_GIC_ERROR_NODE) {
		devarch_base = ioremap(node->info->interface_hdr->address +
					       GIC_ERRDEVARCH,
				       PAGE_SIZE);
		if (!devarch_base)
			return 0;

		reg = readl_relaxed(devarch_base);
		iounmap(devarch_base);

		return FIELD_GET(ERRDEVARCH_REV, reg);
	}

	return FIELD_GET(ID_AA64PFR0_EL1_RAS_MASK, read_cpuid(ID_AA64PFR0_EL1));
}

static int aest_init_record(struct aest_record *record, int i,
			    struct aest_node *node)
{
	struct device *dev = node->adev->dev;

	record->name = devm_kasprintf(dev, GFP_KERNEL, "record%d", i);
	if (!record->name)
		return -ENOMEM;

	if (node->base)
		record->regs_base =
			node->base + sizeof(struct ras_ext_regs) * i;

	record->access = &aest_access[node->info->interface_hdr->type];
	record->addressing_mode = test_bit(i, node->info->addressing_mode);
	record->index = i;
	record->node = node;

	aest_record_dbg(record, "base: %p, index: %d, address mode: %x\n",
			record->regs_base, record->index,
			record->addressing_mode);
	return 0;
}

static void aest_device_remove(struct platform_device *pdev)
{
	platform_set_drvdata(pdev, NULL);
}

static char *alloc_aest_node_name(struct aest_node *node)
{
	char *name;

	switch (node->type) {
	case ACPI_AEST_PROCESSOR_ERROR_NODE:
		name = devm_kasprintf(node->adev->dev, GFP_KERNEL, "%s.%d",
				      aest_node_name[node->type],
				      node->info->processor->processor_id);
		break;
	case ACPI_AEST_MEMORY_ERROR_NODE:
	case ACPI_AEST_SMMU_ERROR_NODE:
	case ACPI_AEST_VENDOR_ERROR_NODE:
	case ACPI_AEST_GIC_ERROR_NODE:
	case ACPI_AEST_PCIE_ERROR_NODE:
	case ACPI_AEST_PROXY_ERROR_NODE:
		name = devm_kasprintf(node->adev->dev, GFP_KERNEL, "%s.%llx",
				      aest_node_name[node->type],
				      node->info->interface_hdr->address);
		break;
	default:
		name = devm_kasprintf(node->adev->dev, GFP_KERNEL, "Unknown");
	}

	return name;
}

static int aest_node_set_errgsr(struct aest_device *adev,
				struct aest_node *node)
{
	struct acpi_aest_node *anode = node->info;
	u64 errgsr_base = anode->common->error_group_register_base;

	if (anode->interface_hdr->type != ACPI_AEST_NODE_MEMORY_MAPPED)
		return 0;

	if (!node->base)
		return 0;

	if (!(anode->interface_hdr->flags & AEST_XFACE_FLAG_ERROR_GROUP)) {
		node->errgsr = node->base + node->group->errgsr_offset;
		return 0;
	}

	if (!errgsr_base)
		return -EINVAL;

	node->errgsr = devm_ioremap(adev->dev, errgsr_base, PAGE_SIZE);
	if (!node->errgsr)
		return -ENOMEM;

	return 0;
}

static int aest_init_node(struct aest_device *adev, struct aest_node *node,
			  struct acpi_aest_node *anode)
{
	int i, ret;
	u64 address, flags;

	node->adev = adev;
	node->info = anode;
	node->type = anode->type;
	node->version = get_aest_node_ver(node);
	node->name = alloc_aest_node_name(node);
	if (!node->name)
		return -ENOMEM;
	node->record_implemented = anode->record_implemented;
	node->status_reporting = anode->status_reporting;
	node->group = &aest_group_config[anode->interface_hdr->group_format];

	address = anode->interface_hdr->address;
	if (address) {
		node->base =
			devm_ioremap(adev->dev, address, node->group->size);
		if (!node->base)
			return -ENOMEM;
	}

	flags = anode->interface_hdr->flags;
	address = node->info->common->fault_inject_register_base;
	if ((flags & AEST_XFACE_FLAG_FAULT_INJECT) && address) {
		if (address - anode->interface_hdr->address < node->group->size)
			node->inj = node->base +
				    (address - anode->interface_hdr->address);
		else {
			node->inj = devm_ioremap(adev->dev, address, PAGE_SIZE);
			if (!node->inj)
				return -ENOMEM;
		}
	}

	ret = aest_node_set_errgsr(adev, node);
	if (ret)
		return ret;

	node->record_count = anode->interface_hdr->error_record_count;
	node->records = devm_kcalloc(adev->dev, node->record_count,
				     sizeof(struct aest_record), GFP_KERNEL);
	if (!node->records)
		return -ENOMEM;

	for (i = 0; i < node->record_count; i++) {
		ret = aest_init_record(&node->records[i], i, node);
		if (ret)
			return ret;
	}
	aest_node_dbg(node, "%d records, base: %llx, errgsr: %llx\n",
		      node->record_count, (u64)node->base, (u64)node->errgsr);
	return 0;
}

static int aest_init_nodes(struct aest_device *adev, struct aest_hnode *ahnode)
{
	struct acpi_aest_node *anode;
	struct aest_node *node;
	int ret, i = 0;

	adev->node_cnt = ahnode->count;
	adev->nodes = devm_kcalloc(adev->dev, adev->node_cnt,
				   sizeof(struct aest_node), GFP_KERNEL);
	if (!adev->nodes)
		return -ENOMEM;

	list_for_each_entry(anode, &ahnode->list, list) {
		adev->type = anode->type;

		node = &adev->nodes[i++];
		ret = aest_init_node(adev, node, anode);
		if (ret)
			return ret;
	}

	return 0;
}

static int aest_device_probe(struct platform_device *pdev)
{
	int ret;
	struct aest_device *adev;
	struct aest_hnode *ahnode;

	ahnode = *((struct aest_hnode **)pdev->dev.platform_data);
	if (!ahnode)
		return -ENODEV;

	adev = devm_kzalloc(&pdev->dev, sizeof(*adev), GFP_KERNEL);
	if (!adev)
		return -ENOMEM;

	adev->dev = &pdev->dev;
	adev->id = pdev->id;
	aest_set_name(adev, ahnode);
	ret = aest_init_nodes(adev, ahnode);
	if (ret)
		return ret;

	platform_set_drvdata(pdev, adev);

	aest_dev_dbg(adev, "Node cnt: %x, id: %x\n", adev->node_cnt, adev->id);

	return 0;
}

static struct platform_driver aest_driver = {
	.driver	= {
		.name	= "AEST",
	},
	.probe	= aest_device_probe,
	.remove = aest_device_remove,
};

static int __init aest_init(void)
{
	return platform_driver_register(&aest_driver);
}
module_init(aest_init);

static void __exit aest_exit(void)
{
	platform_driver_unregister(&aest_driver);
}
module_exit(aest_exit);

MODULE_DESCRIPTION("ARM AEST Driver");
MODULE_AUTHOR("Ruidong Tian <tianruidong@linux.alibaba.com>");
MODULE_LICENSE("GPL");
