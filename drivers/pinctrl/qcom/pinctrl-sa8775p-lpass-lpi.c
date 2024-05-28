// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 * ALSA SoC platform-machine driver for QTi LPASS
 */

#include <linux/gpio/driver.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include "pinctrl-lpass-lpi.h"

enum lpass_lpi_functions {
	LPI_MUX_dmic1_clk,
	LPI_MUX_dmic1_data,
	LPI_MUX_dmic2_clk,
	LPI_MUX_dmic2_data,
	LPI_MUX_dmic3_clk,
	LPI_MUX_dmic3_data,
	LPI_MUX_dmic4_clk,
	LPI_MUX_dmic4_data,
	LPI_MUX_i2s1_clk,
	LPI_MUX_i2s1_data,
	LPI_MUX_i2s1_ws,
	LPI_MUX_i2s2_clk,
	LPI_MUX_i2s2_data,
	LPI_MUX_i2s2_ws,
	LPI_MUX_i2s3_clk,
	LPI_MUX_i2s3_data,
	LPI_MUX_i2s3_ws,
	LPI_MUX_i2s4_clk,
	LPI_MUX_i2s4_data,
	LPI_MUX_i2s4_ws,
	LPI_MUX_qua_mi2s_data,
	LPI_MUX_qua_mi2s_sclk,
	LPI_MUX_qua_mi2s_ws,
	LPI_MUX_ext_mclk1_a,
	LPI_MUX_ext_mclk1_b,
	LPI_MUX_ext_mclk1_c,
	LPI_MUX_ext_mclk1_d,
	LPI_MUX_ext_mclk1_e,
	LPI_MUX_gpio,
	LPI_MUX__,
};

static int gpio0_pins[] = { 0 };
static int gpio1_pins[] = { 1 };
static int gpio2_pins[] = { 2 };
static int gpio3_pins[] = { 3 };
static int gpio4_pins[] = { 4 };
static int gpio5_pins[] = { 5 };
static int gpio6_pins[] = { 6 };
static int gpio7_pins[] = { 7 };
static int gpio8_pins[] = { 8 };
static int gpio9_pins[] = { 9 };
static int gpio10_pins[] = { 10 };
static int gpio11_pins[] = { 11 };
static int gpio12_pins[] = { 12 };
static int gpio13_pins[] = { 13 };
static int gpio14_pins[] = { 14 };
static int gpio15_pins[] = { 15 };
static int gpio16_pins[] = { 16 };
static int gpio17_pins[] = { 17 };
static int gpio18_pins[] = { 18 };
static int gpio19_pins[] = { 19 };
static int gpio20_pins[] = { 20 };
static int gpio21_pins[] = { 21 };
static int gpio22_pins[] = { 22 };

static const struct pinctrl_pin_desc sa8775p_lpi_pins[] = {
	PINCTRL_PIN(0, "gpio0"),
	PINCTRL_PIN(1, "gpio1"),
	PINCTRL_PIN(2, "gpio2"),
	PINCTRL_PIN(3, "gpio3"),
	PINCTRL_PIN(4, "gpio4"),
	PINCTRL_PIN(5, "gpio5"),
	PINCTRL_PIN(6, "gpio6"),
	PINCTRL_PIN(7, "gpio7"),
	PINCTRL_PIN(8, "gpio8"),
	PINCTRL_PIN(9, "gpio9"),
	PINCTRL_PIN(10, "gpio10"),
	PINCTRL_PIN(11, "gpio11"),
	PINCTRL_PIN(12, "gpio12"),
	PINCTRL_PIN(13, "gpio13"),
	PINCTRL_PIN(14, "gpio14"),
	PINCTRL_PIN(15, "gpio15"),
	PINCTRL_PIN(16, "gpio16"),
	PINCTRL_PIN(17, "gpio17"),
	PINCTRL_PIN(18, "gpio18"),
	PINCTRL_PIN(19, "gpio19"),
	PINCTRL_PIN(20, "gpio20"),
	PINCTRL_PIN(21, "gpio21"),
	PINCTRL_PIN(22, "gpio22"),
};

static const char * const dmic1_clk_groups[] = { "gpio6" };
static const char * const dmic1_data_groups[] = { "gpio7" };
static const char * const dmic2_clk_groups[] = { "gpio8" };
static const char * const dmic2_data_groups[] = { "gpio9" };
static const char * const i2s2_clk_groups[] = { "gpio10" };
static const char * const i2s2_ws_groups[] = { "gpio11" };
static const char * const dmic3_clk_groups[] = { "gpio12" };
static const char * const dmic3_data_groups[] = { "gpio13" };
static const char * const dmic4_clk_groups[] = { "gpio17" };
static const char * const dmic4_data_groups[] = { "gpio18" };
static const char * const qua_mi2s_sclk_groups[] = { "gpio0" };
static const char * const qua_mi2s_ws_groups[] = { "gpio1" };
static const char * const qua_mi2s_data_groups[] = { "gpio2", "gpio3", "gpio4", "gpio5" };
static const char * const i2s1_clk_groups[] = { "gpio6" };
static const char * const i2s1_ws_groups[] = { "gpio7" };
static const char * const i2s1_data_groups[] = { "gpio8", "gpio9" };
static const char * const i2s4_clk_groups[] = { "gpio12" };
static const char * const i2s4_ws_groups[] = { "gpio13" };
static const char * const i2s2_data_groups[] = { "gpio15", "gpio16" };
static const char * const i2s3_clk_groups[] = { "gpio19" };
static const char * const i2s3_ws_groups[] = { "gpio20" };
static const char * const i2s3_data_groups[] = { "gpio21", "gpio22" };
static const char * const i2s4_data_groups[] = { "gpio17", "gpio18" };
static const char * const ext_mclk1_c_groups[] = { "gpio5" };
static const char * const ext_mclk1_b_groups[] = { "gpio9" };
static const char * const ext_mclk1_a_groups[] = { "gpio13" };
static const char * const ext_mclk1_d_groups[] = { "gpio14" };
static const char * const ext_mclk1_e_groups[] = { "gpio22" };

static const struct lpi_pingroup sa8775p_groups[] = {
	LPI_PINGROUP(0, LPI_NO_SLEW, qua_mi2s_sclk, _, _, _),
	LPI_PINGROUP(1, LPI_NO_SLEW, qua_mi2s_ws, _, _, _),
	LPI_PINGROUP(2, LPI_NO_SLEW, qua_mi2s_data, _, _, _),
	LPI_PINGROUP(3, LPI_NO_SLEW, qua_mi2s_data, _, _, _),
	LPI_PINGROUP(4, LPI_NO_SLEW, qua_mi2s_data, _, _, _),
	LPI_PINGROUP(5, LPI_NO_SLEW, ext_mclk1_c, qua_mi2s_data, _, _),
	LPI_PINGROUP(6, LPI_NO_SLEW, dmic1_clk, i2s1_clk, _, _),
	LPI_PINGROUP(7, LPI_NO_SLEW, dmic1_data, i2s1_ws, _, _),
	LPI_PINGROUP(8, LPI_NO_SLEW, dmic2_clk, i2s1_data, _, _),
	LPI_PINGROUP(9, LPI_NO_SLEW, dmic2_data, i2s1_data, ext_mclk1_b, _),
	LPI_PINGROUP(10, LPI_NO_SLEW, i2s2_clk, _, _, _),
	LPI_PINGROUP(11, LPI_NO_SLEW, i2s2_ws, _, _, _),
	LPI_PINGROUP(12, LPI_NO_SLEW, dmic3_clk, i2s4_clk, _, _),
	LPI_PINGROUP(13, LPI_NO_SLEW, dmic3_data, i2s4_ws, ext_mclk1_a, _),
	LPI_PINGROUP(14, LPI_NO_SLEW, ext_mclk1_d, _, _, _),
	LPI_PINGROUP(15, LPI_NO_SLEW, i2s2_data, _, _, _),
	LPI_PINGROUP(16, LPI_NO_SLEW, i2s2_data, _, _, _),
	LPI_PINGROUP(17, LPI_NO_SLEW, dmic4_clk, i2s4_data, _, _),
	LPI_PINGROUP(18, LPI_NO_SLEW, dmic4_data, i2s4_data, _, _),
	LPI_PINGROUP(19, LPI_NO_SLEW, i2s3_clk, _, _, _),
	LPI_PINGROUP(20, LPI_NO_SLEW, i2s3_ws, _, _, _),
	LPI_PINGROUP(21, LPI_NO_SLEW, i2s3_data, _, _, _),
	LPI_PINGROUP(22, LPI_NO_SLEW, i2s3_data, ext_mclk1_e, _, _),
};

static const struct lpi_function sa8775p_functions[] = {
	LPI_FUNCTION(dmic1_clk),
	LPI_FUNCTION(dmic1_data),
	LPI_FUNCTION(dmic2_clk),
	LPI_FUNCTION(dmic2_data),
	LPI_FUNCTION(dmic3_clk),
	LPI_FUNCTION(dmic3_data),
	LPI_FUNCTION(dmic4_clk),
	LPI_FUNCTION(dmic4_data),
	LPI_FUNCTION(i2s1_clk),
	LPI_FUNCTION(i2s1_data),
	LPI_FUNCTION(i2s1_ws),
	LPI_FUNCTION(i2s2_clk),
	LPI_FUNCTION(i2s2_data),
	LPI_FUNCTION(i2s2_ws),
	LPI_FUNCTION(i2s3_clk),
	LPI_FUNCTION(i2s3_data),
	LPI_FUNCTION(i2s3_ws),
	LPI_FUNCTION(i2s4_clk),
	LPI_FUNCTION(i2s4_data),
	LPI_FUNCTION(i2s4_ws),
	LPI_FUNCTION(qua_mi2s_data),
	LPI_FUNCTION(qua_mi2s_sclk),
	LPI_FUNCTION(qua_mi2s_ws),
	LPI_FUNCTION(ext_mclk1_a),
	LPI_FUNCTION(ext_mclk1_b),
	LPI_FUNCTION(ext_mclk1_c),
	LPI_FUNCTION(ext_mclk1_d),
	LPI_FUNCTION(ext_mclk1_e),
};

static const struct lpi_pinctrl_variant_data sa8775p_lpi_data = {
	.pins = sa8775p_lpi_pins,
	.npins = ARRAY_SIZE(sa8775p_lpi_pins),
	.groups = sa8775p_groups,
	.ngroups = ARRAY_SIZE(sa8775p_groups),
	.functions = sa8775p_functions,
	.nfunctions = ARRAY_SIZE(sa8775p_functions),
};

static const struct of_device_id lpi_pinctrl_of_match[] = {
	{
	       .compatible = "qcom,sa8775p-lpass-lpi-pinctrl",
	       .data = &sa8775p_lpi_data,
	},
	{
	       .compatible = "qcom,qcs9100-lpass-lpi-pinctrl",
	       .data = &sa8775p_lpi_data,
	},

	{ }
};
MODULE_DEVICE_TABLE(of, lpi_pinctrl_of_match);

static const struct dev_pm_ops lpi_pinctrl_pm_ops = {
	SET_RUNTIME_PM_OPS(lpi_pinctrl_runtime_suspend, lpi_pinctrl_runtime_resume, NULL)
};

static struct platform_driver lpi_pinctrl_driver = {
	.driver = {
		   .name = "qcom-sa8775p-lpass-lpi-pinctrl",
		   .of_match_table = lpi_pinctrl_of_match,
		   .pm = &lpi_pinctrl_pm_ops,
	},
	.probe = lpi_pinctrl_probe,
	.remove = lpi_pinctrl_remove,
};

module_platform_driver(lpi_pinctrl_driver);
MODULE_DESCRIPTION("QTI SA8775p LPI GPIO pin control driver");
MODULE_LICENSE("GPL");
