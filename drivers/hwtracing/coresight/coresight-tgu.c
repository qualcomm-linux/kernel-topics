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

static void tgu_write_all_hw_regs(struct tgu_drvdata *drvdata)
{
	CS_UNLOCK(drvdata->base);
	/* Enable TGU to program the triggers */
	tgu_writel(drvdata, 1, TGU_CONTROL);
	CS_LOCK(drvdata->base);
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
