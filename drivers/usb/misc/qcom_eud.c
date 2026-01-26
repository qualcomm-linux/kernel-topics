// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/bitops.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/usb/role.h>
#include <linux/firmware/qcom/qcom_scm.h>

#define EUD_REG_INT1_EN_MASK	0x0024
#define EUD_REG_INT_STATUS_1	0x0044
#define EUD_REG_CTL_OUT_1	0x0074
#define EUD_REG_VBUS_INT_CLR	0x0080
#define EUD_REG_CSR_EUD_EN	0x1014
#define EUD_REG_SW_ATTACH_DET	0x1018
#define EUD_REG_PORT_SEL	0x1028
#define EUD_REG_EUD_EN2		0x0000

#define EUD_MAX_PORTS		2

#define EUD_ENABLE		BIT(0)
#define EUD_INT_PET_EUD		BIT(0)
#define EUD_INT_VBUS		BIT(2)
#define EUD_INT_SAFE_MODE	BIT(4)
#define EUD_INT_ALL		(EUD_INT_VBUS | EUD_INT_SAFE_MODE)

struct eud_path {
	struct eud_chip		*chip;
	struct phy		*phy;
	struct usb_role_switch	*controller_sw;
	struct usb_role_switch	*eud_sw;
	enum usb_role		curr_role;
	char			name[16];
	u8			num;
};

struct eud_chip {
	struct device			*dev;
	void __iomem			*base;
	struct eud_path			*paths[EUD_MAX_PORTS];
	/* serializes EUD control operations */
	struct mutex			state_lock;
	phys_addr_t			mode_mgr;
	unsigned int			int_status;
	int				irq;
	bool				enabled;
	bool				usb_attached;
	bool				phy_enabled;
	bool				eud_disabled_for_host;
	u8				port_idx;
};

static int eud_phy_enable(struct eud_chip *chip)
{
	struct eud_path *path;
	struct phy *phy;
	int ret;

	if (chip->phy_enabled)
		return 0;

	path = chip->paths[chip->port_idx];
	if (!path || !path->phy) {
		dev_err(chip->dev, "No PHY configured for port %u\n", chip->port_idx);
		return -ENODEV;
	}

	phy = path->phy;

	ret = phy_init(phy);
	if (ret) {
		dev_err(chip->dev, "Failed to initialize USB2 PHY for port %u: %d\n",
			chip->port_idx, ret);
		return ret;
	}

	ret = phy_power_on(phy);
	if (ret) {
		dev_err(chip->dev, "Failed to power on USB2 PHY for port %u: %d\n",
			chip->port_idx, ret);
		phy_exit(phy);
		return ret;
	}

	chip->phy_enabled = true;

	return 0;
}

static void eud_phy_disable(struct eud_chip *chip)
{
	struct eud_path *path;
	struct phy *phy;

	if (!chip->phy_enabled)
		return;

	path = chip->paths[chip->port_idx];
	if (!path || !path->phy)
		return;

	phy = path->phy;

	phy_power_off(phy);
	phy_exit(phy);
	chip->phy_enabled = false;
}

static int enable_eud(struct eud_chip *priv)
{
	int ret;

	ret = eud_phy_enable(priv);
	if (ret)
		return ret;

	ret = qcom_scm_io_writel(priv->mode_mgr + EUD_REG_EUD_EN2, 1);
	if (ret) {
		eud_phy_disable(priv);
		return ret;
	}

	writel(EUD_ENABLE, priv->base + EUD_REG_CSR_EUD_EN);
	writel(EUD_INT_VBUS | EUD_INT_SAFE_MODE,
			priv->base + EUD_REG_INT1_EN_MASK);

	return 0;
}

static int disable_eud(struct eud_chip *priv)
{
	int ret;

	ret = qcom_scm_io_writel(priv->mode_mgr + EUD_REG_EUD_EN2, 0);
	if (ret)
		return ret;

	writel(0, priv->base + EUD_REG_CSR_EUD_EN);
	eud_phy_disable(priv);

	return 0;
}

static ssize_t enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct eud_chip *chip = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%d\n", chip->enabled);
}

static ssize_t enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct eud_chip *chip = dev_get_drvdata(dev);
	struct eud_path *path;
	bool enable;
	int ret;

	if (kstrtobool(buf, &enable))
		return -EINVAL;

	mutex_lock(&chip->state_lock);

	/* Skip operation if already in desired state */
	if (chip->enabled == enable) {
		mutex_unlock(&chip->state_lock);
		return count;
	}

	/*
	 * Handle double-disable scenario: User is disabling EUD that was already
	 * disabled due to host mode. Since the hardware is already disabled, we
	 * only need to clear the host-disabled flag to prevent unwanted re-enabling
	 * when exiting host mode. This respects the user's explicit disable request.
	 */
	if (!enable && chip->eud_disabled_for_host) {
		chip->eud_disabled_for_host = false;
		chip->enabled = false;
		mutex_unlock(&chip->state_lock);
		return count;
	}

	if (enable) {
		/*
		 * EUD functions by presenting itself as a USB device to the host PC for
		 * debugging, making it incompatible in USB host mode configuration.
		 * Prevent enabling EUD in this configuration to avoid hardware conflicts.
		 */
		path = chip->paths[chip->port_idx];
		if (path && path->curr_role == USB_ROLE_HOST) {
			dev_err(chip->dev, "EUD not usable in host mode configuration\n");
			mutex_unlock(&chip->state_lock);
			return -EBUSY;
		}

		ret = enable_eud(chip);
		if (ret) {
			dev_err(chip->dev, "failed to enable eud\n");
			mutex_unlock(&chip->state_lock);
			return ret;
		}
	} else {
		ret = disable_eud(chip);
		if (ret) {
			dev_err(chip->dev, "failed to disable eud\n");
			mutex_unlock(&chip->state_lock);
			return ret;
		}
	}

	chip->enabled = enable;

	mutex_unlock(&chip->state_lock);

	return count;
}

static DEVICE_ATTR_RW(enable);

static ssize_t port_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct eud_chip *chip = dev_get_drvdata(dev);

	return sysfs_emit(buf, "%u\n", chip->port_idx);
}

static ssize_t port_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct eud_chip *chip = dev_get_drvdata(dev);
	u8 port;
	int ret;

	ret = kstrtou8(buf, 0, &port);
	if (ret)
		return ret;

	/* Only port 0 and port 1 are valid */
	if (port >= EUD_MAX_PORTS)
		return -EINVAL;

	/* Check if the corresponding path is available */
	if (!chip->paths[port]) {
		dev_err(chip->dev, "EUD not supported on selected port\n");
		return -EOPNOTSUPP;
	}

	/* Port selection must be done before enabling EUD */
	if (chip->enabled) {
		dev_err(chip->dev, "Cannot change port while EUD is enabled\n");
		return -EBUSY;
	}

	writel(port, chip->base + EUD_REG_PORT_SEL);
	chip->port_idx = port;

	return count;
}

static DEVICE_ATTR_RW(port);

static struct attribute *eud_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_port.attr,
	NULL,
};
ATTRIBUTE_GROUPS(eud);

static void usb_attach_detach(struct eud_chip *chip)
{
	u32 reg;

	/* read ctl_out_1[4] to find USB attach or detach event */
	reg = readl(chip->base + EUD_REG_CTL_OUT_1);
	chip->usb_attached = reg & EUD_INT_SAFE_MODE;
}

static void pet_eud(struct eud_chip *chip)
{
	u32 reg;
	int ret;

	/* When the EUD_INT_PET_EUD in SW_ATTACH_DET is set, the cable has been
	 * disconnected and we need to detach the pet to check if EUD is in safe
	 * mode before attaching again.
	 */
	reg = readl(chip->base + EUD_REG_SW_ATTACH_DET);
	if (reg & EUD_INT_PET_EUD) {
		/* Detach & Attach pet for EUD */
		writel(0, chip->base + EUD_REG_SW_ATTACH_DET);
		/* Delay to make sure detach pet is done before attach pet */
		ret = readl_poll_timeout(chip->base + EUD_REG_SW_ATTACH_DET,
					reg, (reg == 0), 1, 100);
		if (ret) {
			dev_err(chip->dev, "Detach pet failed\n");
			return;
		}
	}
	/* Attach pet for EUD */
	writel(EUD_INT_PET_EUD, chip->base + EUD_REG_SW_ATTACH_DET);
}

static irqreturn_t handle_eud_irq(int irq, void *data)
{
	struct eud_chip *chip = data;
	u32 reg;

	reg = readl(chip->base + EUD_REG_INT_STATUS_1);
	switch (reg & EUD_INT_ALL) {
	case EUD_INT_VBUS:
		usb_attach_detach(chip);
		return IRQ_WAKE_THREAD;
	case EUD_INT_SAFE_MODE:
		pet_eud(chip);
		return IRQ_HANDLED;
	default:
		return IRQ_NONE;
	}
}

static irqreturn_t handle_eud_irq_thread(int irq, void *data)
{
	struct eud_chip *chip = data;
	struct eud_path *path;
	int ret;

	path = chip->paths[chip->port_idx];
	if (!path || !path->controller_sw)
		goto clear_irq;

	/*
	 * EUD virtual attach/detach event handling for low power debugging:
	 *
	 * When EUD is enabled in debug mode, the device remains physically
	 * connected to the PC throughout the debug session, keeping the USB
	 * controller active. This prevents testing of low power scenarios that
	 * require USB disconnection.
	 *
	 * EUD solves this by providing virtual USB attach/detach events while
	 * maintaining the physical connection. These events are triggered from
	 * the Host PC via the enumerated EUD control interface and delivered
	 * to the EUD driver as interrupts.
	 *
	 * These notifications are forwarded to the USB controller through role
	 * switch framework.
	 */
	if (chip->usb_attached)
		ret = usb_role_switch_set_role(path->controller_sw, USB_ROLE_DEVICE);
	else
		ret = usb_role_switch_set_role(path->controller_sw, USB_ROLE_NONE);
	if (ret)
		dev_err(chip->dev, "failed to set role switch\n");

clear_irq:
	/* set and clear vbus_int_clr[0] to clear interrupt */
	writel(BIT(0), chip->base + EUD_REG_VBUS_INT_CLR);
	writel(0, chip->base + EUD_REG_VBUS_INT_CLR);

	return IRQ_HANDLED;
}

static int eud_role_switch_set(struct usb_role_switch *sw, enum usb_role role)
{
	struct eud_path *path = usb_role_switch_get_drvdata(sw);
	struct eud_chip *chip = path->chip;
	int ret;

	mutex_lock(&chip->state_lock);

	/*
	 * EUD must be disabled when USB operates in host mode. EUD functions by
	 * presenting itself as a USB device to the host PC for debugging, making
	 * it incompatible in host mode configuration.
	 *
	 * chip->enabled preserves user's sysfs configuration and is not modified
	 * during host mode transitions to maintain user intent.
	 */

	/* Only act if EUD is enabled and this is the active path */
	if (chip->enabled && path->num == chip->port_idx) {
		if (role == USB_ROLE_HOST && !chip->eud_disabled_for_host) {
			ret = disable_eud(chip);
			if (ret) {
				dev_err(chip->dev, "Failed to disable EUD for host mode: %d\n",
					ret);
				mutex_unlock(&chip->state_lock);
				return ret;
			}
			chip->eud_disabled_for_host = true;
		} else if (role != USB_ROLE_HOST && chip->eud_disabled_for_host) {
			ret = enable_eud(chip);
			if (ret) {
				dev_err(chip->dev, "Failed to re-enable EUD after host mode: %d\n",
					ret);
				mutex_unlock(&chip->state_lock);
				return ret;
			}
			chip->eud_disabled_for_host = false;
		}
	}

	/* Forward the role request to the USB controller */
	ret = usb_role_switch_set_role(path->controller_sw, role);
	if (ret) {
		dev_err(path->chip->dev, "Failed to set role %s for port %u: %d\n",
			usb_role_string(role), path->num, ret);
		mutex_unlock(&chip->state_lock);
		return ret;
	}

	path->curr_role = role;

	mutex_unlock(&chip->state_lock);

	return 0;
}

static void eud_role_switch_release(void *data)
{
	struct eud_chip *chip = data;
	int i;

	for (i = 0; i < EUD_MAX_PORTS; i++) {
		struct eud_path *path = chip->paths[i];

		if (!path)
			continue;

		if (path->eud_sw)
			usb_role_switch_unregister(path->eud_sw);
		if (path->controller_sw)
			usb_role_switch_put(path->controller_sw);
	}
}

static int eud_init_path(struct eud_chip *chip, struct device_node *np)
{
	struct usb_role_switch_desc role_sw_desc = {};
	struct eud_path *path;
	u32 path_num;
	int ret;

	ret = of_property_read_u32(np, "reg", &path_num);
	if (ret) {
		dev_err(chip->dev, "Missing 'reg' property in path node\n");
		return ret;
	}

	if (path_num >= EUD_MAX_PORTS) {
		dev_err(chip->dev, "Invalid path number: %u (max %d)\n",
			path_num, EUD_MAX_PORTS - 1);
		return -EINVAL;
	}

	path = devm_kzalloc(chip->dev, sizeof(*path), GFP_KERNEL);
	if (!path)
		return -ENOMEM;

	path->chip = chip;
	path->num = path_num;

	path->phy = devm_of_phy_get(chip->dev, np, NULL);
	if (IS_ERR(path->phy))
		return dev_err_probe(chip->dev, PTR_ERR(path->phy),
				     "Failed to get PHY for path %d\n", path_num);

	chip->paths[path_num] = path;

	path->curr_role = USB_ROLE_NONE;

	if (!of_property_read_bool(np, "usb-role-switch"))
		return 0;

	/* Fetch the USB controller's role switch */
	path->controller_sw = fwnode_usb_role_switch_get(of_fwnode_handle(np));
	if (IS_ERR(path->controller_sw))
		return dev_err_probe(chip->dev, PTR_ERR(path->controller_sw),
				     "Failed to get controller role switch for path %d\n",
				     path_num);

	/* Create a role switch */
	role_sw_desc.fwnode = of_fwnode_handle(np);
	role_sw_desc.set = eud_role_switch_set;
	role_sw_desc.driver_data = path;
	snprintf(path->name, sizeof(path->name), "eud-path%u", path_num);
	role_sw_desc.name = path->name;

	path->eud_sw = usb_role_switch_register(chip->dev, &role_sw_desc);
	if (IS_ERR(path->eud_sw)) {
		dev_err(chip->dev, "Failed to register EUD role switch for path %d: %ld\n",
			path_num, PTR_ERR(path->eud_sw));
		return PTR_ERR(path->eud_sw);
	}

	return 0;
}

static int eud_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *child;
	struct eud_chip *chip;
	struct resource *res;
	int ret;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;

	mutex_init(&chip->state_lock);

	ret = devm_add_action_or_reset(chip->dev, eud_role_switch_release, chip);
	if (ret)
		return ret;

	for_each_child_of_node(np, child) {
		ret = eud_init_path(chip, child);
		if (ret) {
			of_node_put(child);
			return ret;
		}
	}

	/* Primary path is mandatory. Secondary is optional */
	if (!chip->paths[0])
		return -ENODEV;

	chip->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(chip->base))
		return PTR_ERR(chip->base);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (!res)
		return -ENODEV;
	chip->mode_mgr = res->start;

	chip->irq = platform_get_irq(pdev, 0);
	if (chip->irq < 0)
		return chip->irq;

	ret = devm_request_threaded_irq(&pdev->dev, chip->irq, handle_eud_irq,
			handle_eud_irq_thread, IRQF_ONESHOT, NULL, chip);
	if (ret)
		return dev_err_probe(chip->dev, ret, "failed to allocate irq\n");

	enable_irq_wake(chip->irq);

	platform_set_drvdata(pdev, chip);

	return 0;
}

static void eud_remove(struct platform_device *pdev)
{
	struct eud_chip *chip = platform_get_drvdata(pdev);

	if (chip->enabled)
		disable_eud(chip);

	device_init_wakeup(&pdev->dev, false);
	disable_irq_wake(chip->irq);
}

static const struct of_device_id eud_dt_match[] = {
	{ .compatible = "qcom,eud" },
	{ }
};
MODULE_DEVICE_TABLE(of, eud_dt_match);

static struct platform_driver eud_driver = {
	.probe	= eud_probe,
	.remove = eud_remove,
	.driver	= {
		.name = "qcom_eud",
		.dev_groups = eud_groups,
		.of_match_table = eud_dt_match,
	},
};
module_platform_driver(eud_driver);

MODULE_DESCRIPTION("QTI EUD driver");
MODULE_LICENSE("GPL v2");
