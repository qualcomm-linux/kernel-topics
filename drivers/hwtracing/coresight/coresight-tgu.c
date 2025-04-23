// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2024-2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/amba/bus.h>
#include <linux/coresight.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>

#include "coresight-priv.h"
#include "coresight-tgu.h"

DEFINE_CORESIGHT_DEVLIST(tgu_devs, "tgu");

static int calculate_array_location(struct tgu_drvdata *drvdata,
				    int step_index, int operation_index,
				    int reg_index)
{
	int ret;

	ret = operation_index * (drvdata->max_step) *
		      (drvdata->max_reg) +
	      step_index * (drvdata->max_reg) + reg_index;

	return ret;
}

static ssize_t tgu_dataset_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct tgu_drvdata *drvdata = dev_get_drvdata(dev->parent);
	struct tgu_attribute *tgu_attr =
		container_of(attr, struct tgu_attribute, attr);

	return sysfs_emit(buf, "0x%x\n",
			  drvdata->value_table->priority[
					calculate_array_location(
					drvdata, tgu_attr->step_index,
					tgu_attr->operation_index,
					tgu_attr->reg_num)]);
}

static ssize_t tgu_dataset_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf,
				 size_t size)
{
	unsigned long val;

	struct tgu_drvdata *tgu_drvdata = dev_get_drvdata(dev->parent);
	struct tgu_attribute *tgu_attr =
		container_of(attr, struct tgu_attribute, attr);

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;

	guard(spinlock)(&tgu_drvdata->spinlock);
	tgu_drvdata->value_table->priority[calculate_array_location(
		tgu_drvdata, tgu_attr->step_index, tgu_attr->operation_index,
		tgu_attr->reg_num)] = val;

	return size;
}

static umode_t tgu_node_visible(struct kobject *kobject,
				struct attribute *attr,
				int n)
{
	struct device *dev = kobj_to_dev(kobject);
	struct tgu_drvdata *drvdata = dev_get_drvdata(dev->parent);
	int ret = SYSFS_GROUP_INVISIBLE;

	struct device_attribute *dev_attr =
		container_of(attr, struct device_attribute, attr);
	struct tgu_attribute *tgu_attr =
		container_of(dev_attr, struct tgu_attribute, attr);

	if (tgu_attr->step_index < drvdata->max_step) {
		ret = (tgu_attr->reg_num < drvdata->max_reg) ?
			      attr->mode :
			      0;
	}
	return ret;
}

static void tgu_write_all_hw_regs(struct tgu_drvdata *drvdata)
{
	int i, j, k;

	CS_UNLOCK(drvdata->base);

	for (i = 0; i < drvdata->max_step; i++) {
		for (j = 0; j < MAX_PRIORITY; j++) {
			for (k = 0; k < drvdata->max_reg; k++) {
				tgu_writel(drvdata,
					   drvdata->value_table->priority
						   [calculate_array_location(
							drvdata, i, j, k)],
					   PRIORITY_REG_STEP(i, j, k));
			}
		}
	}

	/* Enable TGU to program the triggers */
	tgu_writel(drvdata, 1, TGU_CONTROL);
	CS_LOCK(drvdata->base);
}

static void tgu_set_reg_number(struct tgu_drvdata *drvdata)
{
	int num_sense_input;
	int num_reg;
	u32 devid;

	devid = readl_relaxed(drvdata->base + CORESIGHT_DEVID);

	num_sense_input = TGU_DEVID_SENSE_INPUT(devid);
	if (((num_sense_input * NUMBER_BITS_EACH_SIGNAL) % LENGTH_REGISTER) == 0)
		num_reg = (num_sense_input * NUMBER_BITS_EACH_SIGNAL) / LENGTH_REGISTER;
	else
		num_reg = ((num_sense_input * NUMBER_BITS_EACH_SIGNAL) / LENGTH_REGISTER) + 1;
	drvdata->max_reg = num_reg;
}

static void tgu_set_steps(struct tgu_drvdata *drvdata)
{
	int num_steps;
	u32 devid;

	devid = readl_relaxed(drvdata->base + CORESIGHT_DEVID);

	num_steps = TGU_DEVID_STEPS(devid);

	drvdata->max_step = num_steps;
}

static int tgu_enable(struct coresight_device *csdev, enum cs_mode mode,
		      void *data)
{
	struct tgu_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock(&drvdata->spinlock);

	if (drvdata->enable) {
		spin_unlock(&drvdata->spinlock);
		return -EBUSY;
	}
	tgu_write_all_hw_regs(drvdata);
	drvdata->enable = true;

	spin_unlock(&drvdata->spinlock);
	return 0;
}

static int tgu_disable(struct coresight_device *csdev, void *data)
{
	struct tgu_drvdata *drvdata = dev_get_drvdata(csdev->dev.parent);

	spin_lock(&drvdata->spinlock);
	if (drvdata->enable) {
		CS_UNLOCK(drvdata->base);
		tgu_writel(drvdata, 0, TGU_CONTROL);
		CS_LOCK(drvdata->base);

		drvdata->enable = false;
	}
	spin_unlock(&drvdata->spinlock);
	return 0;
}

static ssize_t enable_tgu_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	bool enabled;

	struct tgu_drvdata *drvdata = dev_get_drvdata(dev->parent);

	spin_lock(&drvdata->spinlock);
	enabled = drvdata->enable;
	spin_unlock(&drvdata->spinlock);

	return sysfs_emit(buf, "%d\n", enabled);
}

/* enable_tgu_store - Configure Trace and Gating Unit (TGU) triggers. */
static ssize_t enable_tgu_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t size)
{
	int ret = 0;
	unsigned long val;
	struct tgu_drvdata *drvdata = dev_get_drvdata(dev->parent);

	ret = kstrtoul(buf, 0, &val);
	if (ret)
		return ret;

	if (val) {
		ret = pm_runtime_resume_and_get(dev->parent);
		if (ret)
			return ret;
		ret = tgu_enable(drvdata->csdev, CS_MODE_SYSFS, NULL);
		if (ret)
			pm_runtime_put(dev->parent);
	} else {
		ret = tgu_disable(drvdata->csdev, NULL);
		pm_runtime_put(dev->parent);
	}

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR_RW(enable_tgu);

static const struct coresight_ops_helper tgu_helper_ops = {
	.enable = tgu_enable,
	.disable = tgu_disable,
};

static const struct coresight_ops tgu_ops = {
	.helper_ops = &tgu_helper_ops,
};

static struct attribute *tgu_common_attrs[] = {
	&dev_attr_enable_tgu.attr,
	NULL,
};

static const struct attribute_group tgu_common_grp = {
	.attrs = tgu_common_attrs,
	{ NULL },
};

static const struct attribute_group *tgu_attr_groups[] = {
	&tgu_common_grp,
	PRIORITY_ATTRIBUTE_GROUP_INIT(0, 0),
	PRIORITY_ATTRIBUTE_GROUP_INIT(0, 1),
	PRIORITY_ATTRIBUTE_GROUP_INIT(0, 2),
	PRIORITY_ATTRIBUTE_GROUP_INIT(0, 3),
	PRIORITY_ATTRIBUTE_GROUP_INIT(1, 0),
	PRIORITY_ATTRIBUTE_GROUP_INIT(1, 1),
	PRIORITY_ATTRIBUTE_GROUP_INIT(1, 2),
	PRIORITY_ATTRIBUTE_GROUP_INIT(1, 3),
	PRIORITY_ATTRIBUTE_GROUP_INIT(2, 0),
	PRIORITY_ATTRIBUTE_GROUP_INIT(2, 1),
	PRIORITY_ATTRIBUTE_GROUP_INIT(2, 2),
	PRIORITY_ATTRIBUTE_GROUP_INIT(2, 3),
	PRIORITY_ATTRIBUTE_GROUP_INIT(3, 0),
	PRIORITY_ATTRIBUTE_GROUP_INIT(3, 1),
	PRIORITY_ATTRIBUTE_GROUP_INIT(3, 2),
	PRIORITY_ATTRIBUTE_GROUP_INIT(3, 3),
	PRIORITY_ATTRIBUTE_GROUP_INIT(4, 0),
	PRIORITY_ATTRIBUTE_GROUP_INIT(4, 1),
	PRIORITY_ATTRIBUTE_GROUP_INIT(4, 2),
	PRIORITY_ATTRIBUTE_GROUP_INIT(4, 3),
	PRIORITY_ATTRIBUTE_GROUP_INIT(5, 0),
	PRIORITY_ATTRIBUTE_GROUP_INIT(5, 1),
	PRIORITY_ATTRIBUTE_GROUP_INIT(5, 2),
	PRIORITY_ATTRIBUTE_GROUP_INIT(5, 3),
	PRIORITY_ATTRIBUTE_GROUP_INIT(6, 0),
	PRIORITY_ATTRIBUTE_GROUP_INIT(6, 1),
	PRIORITY_ATTRIBUTE_GROUP_INIT(6, 2),
	PRIORITY_ATTRIBUTE_GROUP_INIT(6, 3),
	PRIORITY_ATTRIBUTE_GROUP_INIT(7, 0),
	PRIORITY_ATTRIBUTE_GROUP_INIT(7, 1),
	PRIORITY_ATTRIBUTE_GROUP_INIT(7, 2),
	PRIORITY_ATTRIBUTE_GROUP_INIT(7, 3),
	NULL,
};

static int tgu_probe(struct amba_device *adev, const struct amba_id *id)
{
	int ret = 0;
	struct device *dev = &adev->dev;
	struct coresight_desc desc = { 0 };
	struct coresight_platform_data *pdata;
	struct tgu_drvdata *drvdata;

	desc.name = coresight_alloc_device_name(&tgu_devs, dev);
	if (!desc.name)
		return -ENOMEM;

	pdata = coresight_get_platform_data(dev);
	if (IS_ERR(pdata))
		return PTR_ERR(pdata);

	adev->dev.platform_data = pdata;

	drvdata = devm_kzalloc(dev, sizeof(*drvdata), GFP_KERNEL);
	if (!drvdata)
		return -ENOMEM;

	drvdata->dev = &adev->dev;
	dev_set_drvdata(dev, drvdata);

	drvdata->base = devm_ioremap_resource(dev, &adev->res);
	if (!drvdata->base)
		return -ENOMEM;

	spin_lock_init(&drvdata->spinlock);

	tgu_set_reg_number(drvdata);
	tgu_set_steps(drvdata);

	drvdata->value_table =
		devm_kzalloc(dev, sizeof(*drvdata->value_table), GFP_KERNEL);
	if (!drvdata->value_table)
		return -ENOMEM;

	drvdata->value_table->priority = devm_kzalloc(
		dev,
		MAX_PRIORITY * drvdata->max_reg * drvdata->max_step *
			sizeof(*(drvdata->value_table->priority)),
		GFP_KERNEL);

	if (!drvdata->value_table->priority)
		return -ENOMEM;

	drvdata->enable = false;
	desc.type = CORESIGHT_DEV_TYPE_HELPER;
	desc.pdata = adev->dev.platform_data;
	desc.dev = &adev->dev;
	desc.ops = &tgu_ops;
	desc.groups = tgu_attr_groups;

	drvdata->csdev = coresight_register(&desc);
	if (IS_ERR(drvdata->csdev)) {
		ret = PTR_ERR(drvdata->csdev);
		goto err;
	}

	pm_runtime_put(&adev->dev);
	return 0;
err:
	pm_runtime_put(&adev->dev);
	return ret;
}

static void tgu_remove(struct amba_device *adev)
{
	struct tgu_drvdata *drvdata = dev_get_drvdata(&adev->dev);

	coresight_unregister(drvdata->csdev);
}

static const struct amba_id tgu_ids[] = {
	{
		.id = 0x000f0e00,
		.mask = 0x000fffff,
		.data = "TGU",
	},
	{ 0, 0, NULL },
};

MODULE_DEVICE_TABLE(amba, tgu_ids);

static struct amba_driver tgu_driver = {
	.drv = {
		.name = "coresight-tgu",
		.suppress_bind_attrs = true,
	},
	.probe = tgu_probe,
	.remove = tgu_remove,
	.id_table = tgu_ids,
};

module_amba_driver(tgu_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("CoreSight TGU driver");
