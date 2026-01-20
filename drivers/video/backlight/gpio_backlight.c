// SPDX-License-Identifier: GPL-2.0-only
/*
 * gpio_backlight.c - Simple GPIO-controlled backlight
 */

#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_data/gpio_backlight.h>
#include <linux/platform_device.h>
#include <linux/property.h>
#include <linux/slab.h>
#include <linux/bitmap.h>

struct gpio_backlight {
	struct device *dev;
	struct gpio_descs *gpiods;
	unsigned long *bitmap;
};

static int gpio_backlight_update_status(struct backlight_device *bl)
{
	struct gpio_backlight *gbl = bl_get_data(bl);
	unsigned int n = gbl->gpiods->ndescs;
	int br = backlight_get_brightness(bl);

	if (br)
		bitmap_fill(gbl->bitmap, n);
	else
		bitmap_zero(gbl->bitmap, n);

	gpiod_set_array_value_cansleep(n,
				       gbl->gpiods->desc,
				       gbl->gpiods->info,
				       gbl->bitmap);

	return 0;
}

static bool gpio_backlight_controls_device(struct backlight_device *bl,
					   struct device *display_dev)
{
	struct gpio_backlight *gbl = bl_get_data(bl);

	return !gbl->dev || gbl->dev == display_dev;
}

static const struct backlight_ops gpio_backlight_ops = {
	.options	 = BL_CORE_SUSPENDRESUME,
	.update_status	 = gpio_backlight_update_status,
	.controls_device = gpio_backlight_controls_device,
};

static int gpio_backlight_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct gpio_backlight_platform_data *pdata = dev_get_platdata(dev);
	struct device_node *of_node = dev->of_node;
	struct backlight_properties props = { };
	struct backlight_device *bl;
	struct gpio_backlight *gbl;
	bool def_value;
	enum gpiod_flags flags;
	unsigned int n;
	int words;

	gbl = devm_kcalloc(dev, 1, sizeof(*gbl), GFP_KERNEL);
	if (!gbl)
		return -ENOMEM;

	if (pdata)
		gbl->dev = pdata->dev;

	def_value = device_property_read_bool(dev, "default-on");
	flags = def_value ? GPIOD_OUT_HIGH : GPIOD_OUT_LOW;

	gbl->gpiods = devm_gpiod_get_array(dev, NULL, flags);
	if (IS_ERR(gbl->gpiods)) {
		if (PTR_ERR(gbl->gpiods) == -ENODEV)
			return dev_err_probe(dev, -EINVAL,
			"The gpios parameter is missing or invalid\n");
		return PTR_ERR(gbl->gpiods);
	}

	n = gbl->gpiods->ndescs;
	if (!n)
		return dev_err_probe(dev, -EINVAL,
			"No GPIOs provided\n");

	words = BITS_TO_LONGS(n);
	gbl->bitmap = devm_kcalloc(dev, words, sizeof(unsigned long),
				   GFP_KERNEL);
	if (!gbl->bitmap)
		return -ENOMEM;

	props.type = BACKLIGHT_RAW;
	props.max_brightness = 1;
	bl = devm_backlight_device_register(dev, dev_name(dev), dev, gbl,
					    &gpio_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(dev, "failed to register backlight\n");
		return PTR_ERR(bl);
	}

	/* Set the initial power state */
	if (!of_node || !of_node->phandle)
		/* Not booted with device tree or no phandle link to the node */
		bl->props.power = def_value ? BACKLIGHT_POWER_ON
						    : BACKLIGHT_POWER_OFF;
	else if (gpiod_get_value_cansleep(gbl->gpiods->desc[0]) == 0)
		bl->props.power = BACKLIGHT_POWER_OFF;
	else
		bl->props.power = BACKLIGHT_POWER_ON;

	bl->props.brightness = def_value ? 1 : 0;

	gpio_backlight_update_status(bl);

	platform_set_drvdata(pdev, bl);
	return 0;
}

static struct of_device_id gpio_backlight_of_match[] = {
	{ .compatible = "gpio-backlight" },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, gpio_backlight_of_match);

static struct platform_driver gpio_backlight_driver = {
	.driver		= {
		.name		= "gpio-backlight",
		.of_match_table = gpio_backlight_of_match,
	},
	.probe		= gpio_backlight_probe,
};

module_platform_driver(gpio_backlight_driver);

MODULE_AUTHOR("Laurent Pinchart <laurent.pinchart@ideasonboard.com>");
MODULE_DESCRIPTION("GPIO-based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:gpio-backlight");
