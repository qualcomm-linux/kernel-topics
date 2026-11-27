// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/clk/qcom.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,seca-tcsrcc.h>

static const char * const seca_tcsr_qref0_regulators[] = {
	"vdda-qref0-0p9",
	"vdda-qref-1p2",
};

static const char * const seca_tcsr_qref1_regulators[] = {
	"vdda-qref1-0p9",
	"vdda-qref-1p2",
};

static const char * const seca_tcsr_qref2_regulators[] = {
	"vdda-qref2-0p9",
	"vdda-qref-1p2",
};

static const struct regmap_config tcsr_cc_seca_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xf008,
	.fast_io = true,
};

static const struct qcom_clk_ref_desc * const tcsr_cc_seca_clk_descs[] = {
	[TCSR_DP_RX_0_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_dp_rx_0_clkref_en",
		.offset = 0x5008,
		.regulator_names = seca_tcsr_qref2_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref2_regulators),
	},
	[TCSR_DP_TX_0_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_dp_tx_0_clkref_en",
		.offset = 0x6008,
		.regulator_names = seca_tcsr_qref2_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref2_regulators),
	},
	[TCSR_DP_TX_1_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_dp_tx_1_clkref_en",
		.offset = 0x7008,
		.regulator_names = seca_tcsr_qref2_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref2_regulators),
	},
	[TCSR_DP_TX_2_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_dp_tx_2_clkref_en",
		.offset = 0x8008,
		.regulator_names = seca_tcsr_qref2_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref2_regulators),
	},
	[TCSR_PCIE_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_pcie_clkref_en",
		.offset = 0x8,
		.regulator_names = seca_tcsr_qref0_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref0_regulators),
	},
	[TCSR_UFS_0_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_ufs_0_clkref_en",
		.offset = 0x3008,
		.regulator_names = seca_tcsr_qref2_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref2_regulators),
	},
	[TCSR_UFS_1_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_ufs_1_clkref_en",
		.offset = 0x4008,
		.regulator_names = seca_tcsr_qref2_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref2_regulators),
	},
	[TCSR_USB2_0_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_usb2_0_clkref_en",
		.offset = 0xb008,
		.regulator_names = seca_tcsr_qref1_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref1_regulators),
	},
	[TCSR_USB2_1_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_usb2_1_clkref_en",
		.offset = 0xc008,
		.regulator_names = seca_tcsr_qref1_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref1_regulators),
	},
	[TCSR_USB2_2_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_usb2_2_clkref_en",
		.offset = 0xd008,
		.regulator_names = seca_tcsr_qref1_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref1_regulators),
	},
	[TCSR_USB3_0_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_usb3_0_clkref_en",
		.offset = 0x9008,
		.regulator_names = seca_tcsr_qref1_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref1_regulators),
	},
	[TCSR_USB3_1_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_usb3_1_clkref_en",
		.offset = 0xa008,
		.regulator_names = seca_tcsr_qref1_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref1_regulators),
	},
	[TCSR_UX_SGMII_0_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_ux_sgmii_0_clkref_en",
		.offset = 0x1008,
		.regulator_names = seca_tcsr_qref0_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref0_regulators),
	},
	[TCSR_UX_SGMII_1_CLKREF_EN] = &(const struct qcom_clk_ref_desc) {
		.name = "tcsr_ux_sgmii_1_clkref_en",
		.offset = 0x2008,
		.regulator_names = seca_tcsr_qref0_regulators,
		.num_regulators = ARRAY_SIZE(seca_tcsr_qref0_regulators),
	},
};

static const struct of_device_id tcsr_cc_seca_match_table[] = {
	{ .compatible = "qcom,seca-tcsr" },
	{ }
};
MODULE_DEVICE_TABLE(of, tcsr_cc_seca_match_table);

static int tcsr_cc_seca_probe(struct platform_device *pdev)
{
	return qcom_clk_ref_probe(pdev, &tcsr_cc_seca_regmap_config,
				  tcsr_cc_seca_clk_descs,
				  ARRAY_SIZE(tcsr_cc_seca_clk_descs));
}

static struct platform_driver tcsr_cc_seca_driver = {
	.probe = tcsr_cc_seca_probe,
	.driver = {
		.name = "tcsrcc-seca",
		.of_match_table = tcsr_cc_seca_match_table,
	},
};

subsys_platform_driver(tcsr_cc_seca_driver);

MODULE_DESCRIPTION("QTI TCSRCC Seca Driver");
MODULE_LICENSE("GPL");
