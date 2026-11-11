// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017, 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/usb/role.h>

#include <dt-bindings/phy/phy-qcom-qusb2.h>

#define QUSB2PHY_PORT_QC1	0x70
#define VDM_SRC_EN		BIT(4)
#define VDP_SRC_EN		BIT(2)

#define QUSB2PHY_PORT_UTMI_CTRL1	0xC0
#define SUSPEND_N			BIT(5)
#define TERM_SELECT			BIT(4)
#define XCVR_SELECT_FS			BIT(2)
#define OP_MODE_NON_DRIVE		BIT(0)

#define QUSB2PHY_PORT_UTMI_CTRL2	0xC4
#define UTMI_ULPI_SEL			BIT(7)
#define UTMI_TEST_MUX_SEL		BIT(6)

#define QUSB2PHY_PORT_QC2	0x74
#define RDM_UP_EN		BIT(1)
#define RDP_UP_EN		BIT(3)
#define RPUM_LOW_EN		BIT(4)
#define RPUP_LOW_EN		BIT(5)

#define QUSB2PHY_PLL			0x0
#define QUSB2PHY_PLL_TEST		0x04
#define CLK_REF_SEL			BIT(7)

#define QUSB2PHY_PLL_TUNE		0x08
#define QUSB2PHY_PLL_USER_CTL1		0x0c
#define QUSB2PHY_PLL_USER_CTL2		0x10
#define QUSB2PHY_PLL_AUTOPGM_CTL1	0x1c
#define QUSB2PHY_PLL_PWR_CTRL		0x18
#define PLL_RESET_N_CNT_5		0x5
#define PLL_RESET_N			BIT(4)
#define PLL_AUTOPGM_EN			BIT(7)

/* QUSB2PHY_PLL_STATUS register bits */
#define PLL_LOCKED			BIT(5)

/* QUSB2PHY_PLL_PWR_CTRL register bits */
#define REF_BUF_EN			BIT(6)
#define REXT_EN				BIT(4)
#define PLL_BYPASSNL			BIT(1)
#define REXT_TRIM_0			BIT(0)

/* QUSB2PHY_PLL_COMMON_STATUS_ONE register bits */
#define CORE_READY_STATUS		BIT(0)

/* QUSB2PHY_PORT_POWERDOWN register bits */
#define CLAMP_N_EN			BIT(5)
#define FREEZIO_N			BIT(1)
#define POWER_DOWN			BIT(0)

/* QUSB2PHY_PWR_CTRL1 register bits */
#define PWR_CTRL1_VREF_SUPPLY_TRIM	BIT(5)
#define PWR_CTRL1_CLAMP_N_EN		BIT(1)

#define QUSB2PHY_REFCLK_ENABLE		BIT(0)

#define PHY_CLK_SCHEME_SEL		BIT(0)

/* QUSB2PHY_INTR_CTRL register bits */
#define DMSE_INTR_HIGH_SEL			BIT(4)
#define DPSE_INTR_HIGH_SEL			BIT(3)
#define CHG_DET_INTR_EN				BIT(2)
#define DMSE_INTR_EN				BIT(1)
#define DPSE_INTR_EN				BIT(0)

#define QUSB2PHY_PORT_INTR_STATUS	0xF0
#define DMSE_INTR_HIGH			BIT(2)
#define DPSE_INTR_HIGH			BIT(0)

/* QUSB2PHY_PLL_CORE_INPUT_OVERRIDE register bits */
#define CORE_PLL_EN_FROM_RESET			BIT(4)
#define CORE_RESET				BIT(5)
#define CORE_RESET_MUX				BIT(6)

/* QUSB2PHY_IMP_CTRL1 register bits */
#define IMP_RES_OFFSET_MASK			GENMASK(5, 0)
#define IMP_RES_OFFSET_SHIFT			0x0

/* QUSB2PHY_PLL_BIAS_CONTROL_2 register bits */
#define BIAS_CTRL2_RES_OFFSET_MASK		GENMASK(5, 0)
#define BIAS_CTRL2_RES_OFFSET_SHIFT		0x0

/* QUSB2PHY_CHG_CONTROL_2 register bits */
#define CHG_CTRL2_OFFSET_MASK			GENMASK(5, 4)
#define CHG_CTRL2_OFFSET_SHIFT			0x4

/* QUSB2PHY_PORT_TUNE1 register bits */
#define HSTX_TRIM_MASK				GENMASK(7, 4)
#define HSTX_TRIM_SHIFT				0x4
#define PREEMPH_WIDTH_HALF_BIT			BIT(2)
#define PREEMPHASIS_EN_MASK			GENMASK(1, 0)
#define PREEMPHASIS_EN_SHIFT			0x0

/* QUSB2PHY_PORT_TUNE2 register bits */
#define HSDISC_TRIM_MASK			GENMASK(1, 0)
#define HSDISC_TRIM_SHIFT			0x0

#define QUSB2PHY_PLL_ANALOG_CONTROLS_TWO	0x04
#define QUSB2PHY_PLL_CLOCK_INVERTERS		0x18c
#define QUSB2PHY_PLL_CMODE			0x2c
#define QUSB2PHY_PLL_LOCK_DELAY			0x184
#define QUSB2PHY_PLL_DIGITAL_TIMERS_TWO		0xb4
#define QUSB2PHY_PLL_BIAS_CONTROL_1		0x194
#define QUSB2PHY_PLL_BIAS_CONTROL_2		0x198
#define QUSB2PHY_PWR_CTRL2			0x214
#define QUSB2PHY_IMP_CTRL1			0x220
#define QUSB2PHY_IMP_CTRL2			0x224
#define QUSB2PHY_CHG_CTRL2			0x23c

#define CHG_DCD_TIMEOUT_MSEC			750
#define CHG_DCD_POLL_TIME_MSEC			50
#define CHG_PRIMARY_DET_TIME_MSEC		100
#define CHG_SECONDARY_DET_TIME_MSEC		100
#define DP_PULSE_WIDTH_MSEC			200

enum port_state {
	PORT_UNKNOWN,
	PORT_DISCONNECTED,
	PORT_DCD_IN_PROGRESS,
	PORT_PRIMARY_IN_PROGRESS,
	PORT_SECONDARY_IN_PROGRESS,
	PORT_CHG_DET_DONE,
	PORT_HOST_MODE,
};

enum chg_det_state {
	STATE_UNKNOWN,
	STATE_DCD,
	STATE_PRIMARY,
	STATE_SECONDARY,
};

struct qusb2_phy_init_tbl {
	unsigned int offset;
	unsigned int val;
	/*
	 * register part of layout ?
	 * if yes, then offset gives index in the reg-layout
	 */
	int in_layout;
};

#define QUSB2_PHY_INIT_CFG(o, v) \
	{			\
		.offset = o,	\
		.val = v,	\
	}

#define QUSB2_PHY_INIT_CFG_L(o, v) \
	{			\
		.offset = o,	\
		.val = v,	\
		.in_layout = 1,	\
	}

/* set of registers with offsets different per-PHY */
enum qusb2phy_reg_layout {
	QUSB2PHY_PLL_CORE_INPUT_OVERRIDE,
	QUSB2PHY_PLL_STATUS,
	QUSB2PHY_PORT_TUNE1,
	QUSB2PHY_PORT_TUNE2,
	QUSB2PHY_PORT_TUNE3,
	QUSB2PHY_PORT_TUNE4,
	QUSB2PHY_PORT_TUNE5,
	QUSB2PHY_PORT_TEST1,
	QUSB2PHY_PORT_TEST2,
	QUSB2PHY_PORT_POWERDOWN,
	QUSB2PHY_INTR_CTRL,
};

static const struct qusb2_phy_init_tbl ipq6018_init_tbl[] = {
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL, 0x14),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE1, 0xF8),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE2, 0xB3),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE3, 0x83),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE4, 0xC0),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_TUNE, 0x30),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL1, 0x79),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL2, 0x21),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE5, 0x00),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_PWR_CTRL, 0x00),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TEST2, 0x14),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_TEST, 0x80),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_AUTOPGM_CTL1, 0x9F),
};

static const struct qusb2_phy_init_tbl qcs615_init_tbl[] = {
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE1, 0xc8),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE2, 0xb3),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE3, 0x83),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE4, 0xc0),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_TUNE, 0x30),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL1, 0x79),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL2, 0x21),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TEST2, 0x14),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_AUTOPGM_CTL1, 0x9f),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_PWR_CTRL, 0x00),
};

static const unsigned int ipq6018_regs_layout[] = {
	[QUSB2PHY_PLL_STATUS]              = 0x38,
	[QUSB2PHY_PORT_TUNE1]              = 0x80,
	[QUSB2PHY_PORT_TUNE2]              = 0x84,
	[QUSB2PHY_PORT_TUNE3]              = 0x88,
	[QUSB2PHY_PORT_TUNE4]              = 0x8C,
	[QUSB2PHY_PORT_TUNE5]              = 0x90,
	[QUSB2PHY_PORT_TEST1]              = 0x98,
	[QUSB2PHY_PORT_TEST2]              = 0x9C,
	[QUSB2PHY_PORT_POWERDOWN]          = 0xB4,
	[QUSB2PHY_INTR_CTRL]               = 0xBC,
};

static const unsigned int msm8996_regs_layout[] = {
	[QUSB2PHY_PLL_STATUS]		= 0x38,
	[QUSB2PHY_PORT_TUNE1]		= 0x80,
	[QUSB2PHY_PORT_TUNE2]		= 0x84,
	[QUSB2PHY_PORT_TUNE3]		= 0x88,
	[QUSB2PHY_PORT_TUNE4]		= 0x8c,
	[QUSB2PHY_PORT_TUNE5]		= 0x90,
	[QUSB2PHY_PORT_TEST1]		= 0xb8,
	[QUSB2PHY_PORT_TEST2]		= 0x9c,
	[QUSB2PHY_PORT_POWERDOWN]	= 0xb4,
	[QUSB2PHY_INTR_CTRL]		= 0xbc,
};

static const struct qusb2_phy_init_tbl msm8996_init_tbl[] = {
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE1, 0xf8),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE2, 0xb3),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE3, 0x83),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE4, 0xc0),

	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_TUNE, 0x30),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL1, 0x79),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL2, 0x21),

	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TEST2, 0x14),

	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_AUTOPGM_CTL1, 0x9f),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_PWR_CTRL, 0x00),
};

static const unsigned int msm8998_regs_layout[] = {
	[QUSB2PHY_PLL_CORE_INPUT_OVERRIDE] = 0xa8,
	[QUSB2PHY_PLL_STATUS]              = 0x1a0,
	[QUSB2PHY_PORT_TUNE1]              = 0x23c,
	[QUSB2PHY_PORT_TUNE2]              = 0x240,
	[QUSB2PHY_PORT_TUNE3]              = 0x244,
	[QUSB2PHY_PORT_TUNE4]              = 0x248,
	[QUSB2PHY_PORT_TEST1]              = 0x24c,
	[QUSB2PHY_PORT_TEST2]              = 0x250,
	[QUSB2PHY_PORT_POWERDOWN]          = 0x210,
	[QUSB2PHY_INTR_CTRL]               = 0x22c,
};

static const struct qusb2_phy_init_tbl msm8998_init_tbl[] = {
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_ANALOG_CONTROLS_TWO, 0x13),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_CLOCK_INVERTERS, 0x7c),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_CMODE, 0x80),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_LOCK_DELAY, 0x0a),

	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE1, 0xa5),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE2, 0x09),

	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_DIGITAL_TIMERS_TWO, 0x19),
};

static const struct qusb2_phy_init_tbl sm6115_init_tbl[] = {
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE1, 0xf8),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE2, 0x53),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE3, 0x81),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE4, 0x17),

	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_TUNE, 0x30),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL1, 0x79),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_USER_CTL2, 0x21),

	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TEST2, 0x14),

	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_AUTOPGM_CTL1, 0x9f),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_PWR_CTRL, 0x00),
};

static const unsigned int qusb2_v2_regs_layout[] = {
	[QUSB2PHY_PLL_CORE_INPUT_OVERRIDE] = 0xa8,
	[QUSB2PHY_PLL_STATUS]		= 0x1a0,
	[QUSB2PHY_PORT_TUNE1]		= 0x240,
	[QUSB2PHY_PORT_TUNE2]		= 0x244,
	[QUSB2PHY_PORT_TUNE3]		= 0x248,
	[QUSB2PHY_PORT_TUNE4]		= 0x24c,
	[QUSB2PHY_PORT_TUNE5]		= 0x250,
	[QUSB2PHY_PORT_TEST1]		= 0x254,
	[QUSB2PHY_PORT_TEST2]		= 0x258,
	[QUSB2PHY_PORT_POWERDOWN]	= 0x210,
	[QUSB2PHY_INTR_CTRL]		= 0x230,
};

static const struct qusb2_phy_init_tbl qusb2_v2_init_tbl[] = {
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_ANALOG_CONTROLS_TWO, 0x03),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_CLOCK_INVERTERS, 0x7c),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_CMODE, 0x80),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_LOCK_DELAY, 0x0a),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_DIGITAL_TIMERS_TWO, 0x19),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_BIAS_CONTROL_1, 0x40),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PLL_BIAS_CONTROL_2, 0x20),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_PWR_CTRL2, 0x21),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_IMP_CTRL1, 0x0),
	QUSB2_PHY_INIT_CFG(QUSB2PHY_IMP_CTRL2, 0x58),

	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE1, 0x30),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE2, 0x29),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE3, 0xca),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE4, 0x04),
	QUSB2_PHY_INIT_CFG_L(QUSB2PHY_PORT_TUNE5, 0x03),

	QUSB2_PHY_INIT_CFG(QUSB2PHY_CHG_CTRL2, 0x0),
};

struct qusb2_phy_cfg {
	const struct qusb2_phy_init_tbl *tbl;
	/* number of entries in the table */
	unsigned int tbl_num;
	/* offset to PHY_CLK_SCHEME register in TCSR map */
	unsigned int clk_scheme_offset;

	/* array of registers with different offsets */
	const unsigned int *regs;
	unsigned int mask_core_ready;
	unsigned int disable_ctrl;
	unsigned int autoresume_en;

	/* true if PHY has PLL_TEST register to select clk_scheme */
	bool has_pll_test;

	/* true if TUNE1 register must be updated by fused value, else TUNE2 */
	bool update_tune1_with_efuse;

	/* true if PHY has PLL_CORE_INPUT_OVERRIDE register to reset PLL */
	bool has_pll_override;

	/* true if PHY default clk scheme is single-ended */
	bool se_clk_scheme_default;
};

static const struct qusb2_phy_cfg msm8996_phy_cfg = {
	.tbl		= msm8996_init_tbl,
	.tbl_num	= ARRAY_SIZE(msm8996_init_tbl),
	.regs		= msm8996_regs_layout,

	.has_pll_test	= true,
	.se_clk_scheme_default = true,
	.disable_ctrl	= (CLAMP_N_EN | FREEZIO_N | POWER_DOWN),
	.mask_core_ready = PLL_LOCKED,
	.autoresume_en	 = BIT(3),
};

static const struct qusb2_phy_cfg msm8998_phy_cfg = {
	.tbl            = msm8998_init_tbl,
	.tbl_num        = ARRAY_SIZE(msm8998_init_tbl),
	.regs           = msm8998_regs_layout,

	.disable_ctrl   = POWER_DOWN,
	.mask_core_ready = CORE_READY_STATUS,
	.has_pll_override = true,
	.se_clk_scheme_default = true,
	.autoresume_en   = BIT(0),
	.update_tune1_with_efuse = true,
};

static const struct qusb2_phy_cfg ipq6018_phy_cfg = {
	.tbl            = ipq6018_init_tbl,
	.tbl_num        = ARRAY_SIZE(ipq6018_init_tbl),
	.regs           = ipq6018_regs_layout,

	.disable_ctrl   = POWER_DOWN,
	.mask_core_ready = PLL_LOCKED,
	/* autoresume not used */
	.autoresume_en   = BIT(0),
};

static const struct qusb2_phy_cfg qcs615_phy_cfg = {
	.tbl            = qcs615_init_tbl,
	.tbl_num        = ARRAY_SIZE(qcs615_init_tbl),
	.regs           = ipq6018_regs_layout,

	.disable_ctrl   = (CLAMP_N_EN | FREEZIO_N | POWER_DOWN),
	.mask_core_ready = PLL_LOCKED,
	/* autoresume not used */
	.autoresume_en   = BIT(0),
};

static const struct qusb2_phy_cfg qusb2_v2_phy_cfg = {
	.tbl		= qusb2_v2_init_tbl,
	.tbl_num	= ARRAY_SIZE(qusb2_v2_init_tbl),
	.regs		= qusb2_v2_regs_layout,

	.disable_ctrl	= (PWR_CTRL1_VREF_SUPPLY_TRIM | PWR_CTRL1_CLAMP_N_EN |
			   POWER_DOWN),
	.mask_core_ready = CORE_READY_STATUS,
	.has_pll_override = true,
	.se_clk_scheme_default = true,
	.autoresume_en	  = BIT(0),
	.update_tune1_with_efuse = true,
};

static const struct qusb2_phy_cfg sdm660_phy_cfg = {
	.tbl		= msm8996_init_tbl,
	.tbl_num	= ARRAY_SIZE(msm8996_init_tbl),
	.regs		= msm8996_regs_layout,

	.has_pll_test	= true,
	.se_clk_scheme_default = false,
	.disable_ctrl	= (CLAMP_N_EN | FREEZIO_N | POWER_DOWN),
	.mask_core_ready = PLL_LOCKED,
	.autoresume_en	 = BIT(3),
};

static const struct qusb2_phy_cfg sm6115_phy_cfg = {
	.tbl		= sm6115_init_tbl,
	.tbl_num	= ARRAY_SIZE(sm6115_init_tbl),
	.regs		= msm8996_regs_layout,

	.has_pll_test	= true,
	.se_clk_scheme_default = true,
	.disable_ctrl	= (CLAMP_N_EN | FREEZIO_N | POWER_DOWN),
	.mask_core_ready = PLL_LOCKED,
	.autoresume_en	 = BIT(3),
};

static const char * const qusb2_phy_vreg_names[] = {
	"vdd", "vdda-pll", "vdda-phy-dpdm",
};

#define QUSB2_NUM_VREGS		ARRAY_SIZE(qusb2_phy_vreg_names)

/* struct override_param - structure holding qusb2 v2 phy overriding param
 * set override true if the  device tree property exists and read and assign
 * to value
 */
struct override_param {
	bool override;
	u8 value;
};

/*struct override_params - structure holding qusb2 v2 phy overriding params
 * @imp_res_offset: rescode offset to be updated in IMP_CTRL1 register
 * @hstx_trim: HSTX_TRIM to be updated in TUNE1 register
 * @preemphasis: Amplitude Pre-Emphasis to be updated in TUNE1 register
 * @preemphasis_width: half/full-width Pre-Emphasis updated via TUNE1
 * @bias_ctrl: bias ctrl to be updated in BIAS_CONTROL_2 register
 * @charge_ctrl: charge ctrl to be updated in CHG_CTRL2 register
 * @hsdisc_trim: disconnect threshold to be updated in TUNE2 register
 */
struct override_params {
	struct override_param imp_res_offset;
	struct override_param hstx_trim;
	struct override_param preemphasis;
	struct override_param preemphasis_width;
	struct override_param bias_ctrl;
	struct override_param charge_ctrl;
	struct override_param hsdisc_trim;
};

/**
 * struct qusb2_phy - structure holding qusb2 phy attributes
 *
 * @phy: generic phy
 * @base: iomapped memory space for qubs2 phy
 *
 * @cfg_ahb_clk: AHB2PHY interface clock
 * @ref_clk: phy reference clock
 * @iface_clk: phy interface clock
 * @phy_reset: phy reset control
 * @vregs: regulator supplies bulk data
 *
 * @tcsr: TCSR syscon register map
 * @cell: nvmem cell containing phy tuning value
 *
 * @overrides: pointer to structure for all overriding tuning params
 *
 * @cfg: phy config data
 * @has_se_clk_scheme: indicate if PHY has single-ended ref clock scheme
 * @phy_initialized: indicate if PHY has been initialized
 * @mode: current PHY mode
 */
struct qusb2_phy {
	struct phy *phy;
	void __iomem *base;

	struct clk *cfg_ahb_clk;
	struct clk *ref_clk;
	struct clk *iface_clk;
	struct reset_control *phy_reset;
	struct regulator_bulk_data vregs[QUSB2_NUM_VREGS];

	struct regmap *tcsr;
	struct nvmem_cell *cell;

	struct override_params overrides;

	const struct qusb2_phy_cfg *cfg;
	bool has_se_clk_scheme;
	bool phy_initialized;
	bool is_phy_enabled;
	enum phy_mode mode;
	int vbus_active;
	int id_state;
	struct power_supply	*usb_psy;
	struct delayed_work	port_det_w;
	enum port_state		port_state;
	unsigned int		dcd_timeout;
	bool			dcp_charger;

	struct usb_role_switch	*connector_role_sw;
	struct usb_role_switch	*controller_role_sw;
	enum usb_role		current_role;
	struct mutex		role_lock;
};

static int qusb_phy_notify_charger(struct qusb2_phy *qphy,
					enum power_supply_usb_type charger_type)
{
	union power_supply_propval pval = {0};

	if (!qphy->usb_psy) {
		qphy->usb_psy = power_supply_get_by_name("pm4125-charger");
		if (!qphy->usb_psy) {
			dev_err(&qphy->phy->dev, "could not get usb power supply\n");
			return -ENODEV;
		}
	}

	dev_dbg(&qphy->phy->dev, "notifying charger type: %d\n", charger_type);

	pval.intval = charger_type;
	power_supply_set_property(qphy->usb_psy, POWER_SUPPLY_PROP_USB_TYPE,
									&pval);
	return 0;
}

static bool qusb_phy_chg_det_status(struct qusb2_phy *qphy,
						enum chg_det_state state)
{
	u32 reg, status;

	reg = readl_relaxed(qphy->base + QUSB2PHY_PORT_INTR_STATUS);
	dev_dbg(&qphy->phy->dev, "chg_det_status: state=%d intr_status=0x%02x\n",
		state, reg);

	status = reg & 0xff;

	switch (state) {
	case STATE_DCD:
		return (status != DPSE_INTR_HIGH);
	case STATE_PRIMARY:
		return (status && (status != DPSE_INTR_HIGH));
	case STATE_SECONDARY:
		return status;
	case STATE_UNKNOWN:
	default:
		break;
	}

	return false;
}

/*
 * Different circuit blocks are enabled on DP and DM lines as part
 * of different phases of charger detection. Then the state of
 * DP and DM lines are monitored to identify different type of
 * chargers.
 * These circuit blocks can be enabled with the configuration of
 * the QUICKCHARGE1 and QUICKCHARGE2 registers and the DP/DM lines
 * can be monitored with the status of the INTR_STATUS register.
 */
static void qusb_phy_chg_det_enable_seq(struct qusb2_phy *qphy, int state)
{
	dev_dbg(&qphy->phy->dev, "chg_det_enable_seq: state=%d base: %pK\n", state, qphy->base);

	writel_relaxed(0x23, qphy->base + qphy->cfg->regs[QUSB2PHY_PORT_POWERDOWN]);
	writel_relaxed(0x35, qphy->base + QUSB2PHY_PORT_UTMI_CTRL1);
	writel_relaxed(0xC0, qphy->base + QUSB2PHY_PORT_UTMI_CTRL2);
	writel_relaxed(0x05, qphy->base + QUSB2PHY_PLL_AUTOPGM_CTL1);
	writel_relaxed(0x22, qphy->base + qphy->cfg->regs[QUSB2PHY_PORT_POWERDOWN]);
	writel_relaxed(0x17, qphy->base + QUSB2PHY_PLL_PWR_CTRL);

	usleep_range(5, 10);

	writel_relaxed(0x15, qphy->base + QUSB2PHY_PLL_AUTOPGM_CTL1);
	writel_relaxed(0x00, qphy->base + QUSB2PHY_PORT_QC1);
	writel_relaxed(0x00, qphy->base + QUSB2PHY_PORT_QC2);

	usleep_range(50, 60);

	switch (state) {
	case STATE_DCD:
		writel_relaxed(0x08, qphy->base + QUSB2PHY_PORT_QC1);
		writel_relaxed(0x01, qphy->base + QUSB2PHY_PORT_QC2);
		writel_relaxed(0x1F, qphy->base + qphy->cfg->regs[QUSB2PHY_INTR_CTRL]);
		break;
	case STATE_PRIMARY:
		writel_relaxed(0x25, qphy->base + QUSB2PHY_PORT_QC1);
		writel_relaxed(0x00, qphy->base + QUSB2PHY_PORT_QC2);
		writel_relaxed(0x1F, qphy->base + qphy->cfg->regs[QUSB2PHY_INTR_CTRL]);
		break;
	case STATE_SECONDARY:
		writel_relaxed(0x72, qphy->base + QUSB2PHY_PORT_QC1);
		writel_relaxed(0x00, qphy->base + QUSB2PHY_PORT_QC2);
		writel_relaxed(0x1F, qphy->base + qphy->cfg->regs[QUSB2PHY_INTR_CTRL]);
		break;
	case STATE_UNKNOWN:
	default:
		break;
	};
}

/*
 * qusb2_phy_enable_phy - power up regulators and enable clocks for the PHY.
 *
 * Used by the charger-detection path to bring the PHY up before programming
 * the QC1/QC2 detection registers, and by qusb2_phy_init().
 * No-op if the PHY is already enabled (is_phy_enabled is true).
 */
static int qusb2_phy_enable_phy(struct qusb2_phy *qphy)
{
	struct device *dev = &qphy->phy->dev;
	int ret;

	if (qphy->is_phy_enabled) {
		dev_dbg(dev, "PHY already enabled, skipping\n");
		return 0;
	}

	dev_vdbg(dev, "enabling PHY clocks and regulators\n");

	ret = regulator_bulk_enable(ARRAY_SIZE(qphy->vregs), qphy->vregs);
	if (ret) {
		dev_err(dev, "failed to enable regulators, %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(qphy->iface_clk);
	if (ret) {
		dev_err(dev, "failed to enable iface_clk, %d\n", ret);
		goto disable_regs;
	}

	ret = clk_prepare_enable(qphy->cfg_ahb_clk);
	if (ret) {
		dev_err(dev, "failed to enable cfg_ahb_clk, %d\n", ret);
		goto disable_iface_clk;
	}

	if (!qphy->has_se_clk_scheme) {
		ret = clk_prepare_enable(qphy->ref_clk);
		if (ret) {
			dev_err(dev, "failed to enable ref_clk, %d\n", ret);
			goto disable_ahb_clk;
		}
	}

	qphy->is_phy_enabled = true;

	return 0;

disable_ahb_clk:
	clk_disable_unprepare(qphy->cfg_ahb_clk);
disable_iface_clk:
	clk_disable_unprepare(qphy->iface_clk);
disable_regs:
	regulator_bulk_disable(ARRAY_SIZE(qphy->vregs), qphy->vregs);
	return ret;
}

/*
 * qusb2_phy_disable_phy - disable clocks and power down regulators for the PHY.
 */
static void qusb2_phy_disable_phy(struct qusb2_phy *qphy)
{
	if (!qphy->is_phy_enabled) {
		dev_dbg(&qphy->phy->dev, "PHY already disabled, skipping\n");
		return;
	}

	dev_vdbg(&qphy->phy->dev, "disabling PHY clocks and regulators\n");

	if (!qphy->has_se_clk_scheme)
		clk_disable_unprepare(qphy->ref_clk);

	clk_disable_unprepare(qphy->cfg_ahb_clk);
	clk_disable_unprepare(qphy->iface_clk);
	regulator_bulk_disable(ARRAY_SIZE(qphy->vregs), qphy->vregs);
	qphy->is_phy_enabled = false;
}

/*
 * qusb2_phy_forward_role - forward a USB role to the controller.
 *
 * Only called after charger detection confirms the port is SDP or CDP, so
 * the controller knows it should operate in device mode.
 */
static void qusb2_phy_forward_role(struct qusb2_phy *qphy, enum usb_role role)
{
	if (!qphy->controller_role_sw)
		return;

	dev_dbg(&qphy->phy->dev, "forwarding role %s to controller\n",
		usb_role_string(role));

	usb_role_switch_set_role(qphy->controller_role_sw, role);
}

static int qusb_phy_drive_dp_pulse(struct qusb2_phy *qphy)
{
	int ret;

	dev_dbg(&qphy->phy->dev, "connected to a CDP, drive DP up\n");

	ret = qusb2_phy_enable_phy(qphy);
	if (ret)
		return ret;

	ret = reset_control_assert(qphy->phy_reset);
	if (ret)
		dev_err(&qphy->phy->dev, "phy assert failed\n");
	usleep_range(100, 150);
	ret = reset_control_deassert(qphy->phy_reset);
	if (ret)
		dev_err(&qphy->phy->dev, "phy deassert failed\n");

	writel_relaxed(CLAMP_N_EN | FREEZIO_N | POWER_DOWN,
			qphy->base + qphy->cfg->regs[QUSB2PHY_PORT_POWERDOWN]);
	writel_relaxed(TERM_SELECT | XCVR_SELECT_FS | OP_MODE_NON_DRIVE |
			SUSPEND_N, qphy->base + QUSB2PHY_PORT_UTMI_CTRL1);
	writel_relaxed(UTMI_ULPI_SEL | UTMI_TEST_MUX_SEL,
			qphy->base + QUSB2PHY_PORT_UTMI_CTRL2);
	writel_relaxed(PLL_RESET_N_CNT_5,
			qphy->base + QUSB2PHY_PLL_AUTOPGM_CTL1);
	writel_relaxed(CLAMP_N_EN | FREEZIO_N,
			qphy->base + qphy->cfg->regs[QUSB2PHY_PORT_POWERDOWN]);
	writel_relaxed(REF_BUF_EN | REXT_EN | PLL_BYPASSNL | REXT_TRIM_0,
			qphy->base + QUSB2PHY_PLL_PWR_CTRL);

	usleep_range(5, 10);

	writel_relaxed(0x15, qphy->base + QUSB2PHY_PLL_AUTOPGM_CTL1);
	writel_relaxed(PLL_RESET_N | PLL_RESET_N_CNT_5,
			qphy->base + QUSB2PHY_PLL_AUTOPGM_CTL1);
	writel_relaxed(0x00, qphy->base + QUSB2PHY_PORT_QC1);
	writel_relaxed(0x00, qphy->base + QUSB2PHY_PORT_QC2);

	usleep_range(50, 60);

	writel_relaxed(RDP_UP_EN, qphy->base + QUSB2PHY_PORT_QC2);

	msleep(DP_PULSE_WIDTH_MSEC);

	writel_relaxed(CLAMP_N_EN | FREEZIO_N | POWER_DOWN,
			qphy->base + qphy->cfg->regs[QUSB2PHY_PORT_POWERDOWN]);
	writel_relaxed(PLL_AUTOPGM_EN | PLL_RESET_N | PLL_RESET_N_CNT_5,
			qphy->base + QUSB2PHY_PLL_AUTOPGM_CTL1);
	writel_relaxed(UTMI_ULPI_SEL, qphy->base + QUSB2PHY_PORT_UTMI_CTRL2);
	writel_relaxed(TERM_SELECT, qphy->base + QUSB2PHY_PORT_UTMI_CTRL1);

	qusb2_phy_disable_phy(qphy);

	return 0;
}

static void qusb_phy_port_state_work(struct work_struct *w)
{
	struct qusb2_phy *qphy = container_of(w, struct qusb2_phy,
							port_det_w.work);
	unsigned long delay = 0;
	int status, ret = 0;

	dev_dbg(&qphy->phy->dev, "port_state_work: state=%d vbus_active=%d id_state=%d\n",
		qphy->port_state, qphy->vbus_active, qphy->id_state);

	switch (qphy->port_state) {
	case PORT_UNKNOWN:
		if (!qphy->id_state) {
			dev_dbg(&qphy->phy->dev, "ID grounded, entering host mode\n");
			qphy->port_state = PORT_HOST_MODE;
			qusb2_phy_forward_role(qphy, USB_ROLE_HOST);
			return;
		}

		if (qphy->vbus_active) {
			dev_dbg(&qphy->phy->dev, "VBUS active, starting DCD\n");

			/* Enable DCD sequence */
			ret = qusb2_phy_enable_phy(qphy);
			if (ret)
				return;

			qusb_phy_chg_det_enable_seq(qphy, STATE_DCD);
			qphy->port_state = PORT_DCD_IN_PROGRESS;
			qphy->dcd_timeout = 0;
			delay = CHG_DCD_POLL_TIME_MSEC;
			break;
		}
		return;
	case PORT_DISCONNECTED:
		dev_dbg(&qphy->phy->dev, "port disconnected, resetting state\n");
		qusb2_phy_disable_phy(qphy);
		qphy->port_state = PORT_UNKNOWN;
		qphy->dcp_charger = false;
		break;
	case PORT_DCD_IN_PROGRESS:
		if (!qphy->vbus_active) {
			dev_dbg(&qphy->phy->dev, "VBUS lost during DCD, aborting\n");

			/* Disable PHY sequence */
			qusb2_phy_disable_phy(qphy);
			qphy->port_state = PORT_DISCONNECTED;
			break;
		}

		status = qusb_phy_chg_det_status(qphy, STATE_DCD);
		if (!status && qphy->dcd_timeout < CHG_DCD_TIMEOUT_MSEC) {
			dev_dbg(&qphy->phy->dev, "DCD polling: timeout=%u ms\n",
				qphy->dcd_timeout);
			delay = CHG_DCD_POLL_TIME_MSEC;
			qphy->dcd_timeout += delay;
		} else if (status) {
			dev_dbg(&qphy->phy->dev, "DCD complete, starting primary detection\n");
			qusb_phy_chg_det_enable_seq(qphy, STATE_PRIMARY);
			qphy->port_state = PORT_PRIMARY_IN_PROGRESS;
			delay = CHG_PRIMARY_DET_TIME_MSEC;
		} else if (qphy->dcd_timeout >= CHG_DCD_TIMEOUT_MSEC) {
			dev_dbg(&qphy->phy->dev,
				 "charger detected: DCP (DCD timeout after %u ms)\n",
				 qphy->dcd_timeout);
			qusb_phy_notify_charger(qphy,
						POWER_SUPPLY_USB_TYPE_DCP);
			qusb2_phy_disable_phy(qphy);
			qphy->port_state = PORT_CHG_DET_DONE;
		}
		break;
	case PORT_PRIMARY_IN_PROGRESS:
		if (!qphy->vbus_active) {
			dev_dbg(&qphy->phy->dev, "VBUS lost during primary detection, aborting\n");
			qusb2_phy_disable_phy(qphy);
			qphy->port_state = PORT_DISCONNECTED;
			break;
		}

		status = qusb_phy_chg_det_status(qphy, STATE_PRIMARY);
		if (status) {
			dev_dbg(&qphy->phy->dev,
				"primary detection positive, starting secondary detection\n");
			qusb_phy_chg_det_enable_seq(qphy, STATE_SECONDARY);
			qphy->port_state = PORT_SECONDARY_IN_PROGRESS;
			delay = CHG_SECONDARY_DET_TIME_MSEC;

		} else {
			dev_dbg(&qphy->phy->dev, "charger detected: SDP (standard USB host)\n");
			qusb2_phy_disable_phy(qphy);
			qusb_phy_notify_charger(qphy, POWER_SUPPLY_USB_TYPE_SDP);
			qusb2_phy_forward_role(qphy, USB_ROLE_DEVICE);
			qphy->port_state = PORT_CHG_DET_DONE;
		}
		break;
	case PORT_SECONDARY_IN_PROGRESS:
		if (!qphy->vbus_active) {
			dev_dbg(&qphy->phy->dev, "VBUS lost during secondary detection, aborting\n");
			qusb2_phy_disable_phy(qphy);
			qphy->port_state = PORT_DISCONNECTED;
			break;
		}

		status = qusb_phy_chg_det_status(qphy, STATE_SECONDARY);
		if (status) {
			dev_dbg(&qphy->phy->dev, "charger detected: DCP (dedicated charging port)\n");
			qusb_phy_notify_charger(qphy,
						POWER_SUPPLY_USB_TYPE_DCP);
			qphy->dcp_charger = true;
		} else {
			dev_dbg(&qphy->phy->dev, "charger detected: CDP (charging downstream port)\n");
			qusb_phy_notify_charger(qphy,
						POWER_SUPPLY_USB_TYPE_CDP);
			qusb_phy_drive_dp_pulse(qphy);
			qusb2_phy_disable_phy(qphy);
			qusb2_phy_forward_role(qphy, USB_ROLE_DEVICE);
		}

		qphy->port_state = PORT_CHG_DET_DONE;
		break;
	case PORT_CHG_DET_DONE:
		if (!qphy->vbus_active) {
			dev_dbg(&qphy->phy->dev, "VBUS removed, resetting from CHG_DET_DONE\n");

			if (qphy->dcp_charger) {
				qphy->dcp_charger = false;
				qusb2_phy_disable_phy(qphy);
			} else {
				qusb2_phy_forward_role(qphy, USB_ROLE_NONE);
			}
			qphy->port_state = PORT_UNKNOWN;
		}

		return;
	case PORT_HOST_MODE:
		if (qphy->id_state) {
			dev_dbg(&qphy->phy->dev, "ID released, leaving host mode\n");
			qphy->port_state = PORT_UNKNOWN;
			qusb2_phy_forward_role(qphy, USB_ROLE_NONE);
		}

		if (!qphy->vbus_active)
			return;

		break;
	default:
		return;
	}

	queue_delayed_work(system_freezable_wq,
			&qphy->port_det_w, msecs_to_jiffies(delay));
}

static inline void qusb2_write_mask(void __iomem *base, u32 offset,
				    u32 val, u32 mask)
{
	u32 reg;

	reg = readl(base + offset);
	reg &= ~mask;
	reg |= val & mask;
	writel(reg, base + offset);

	/* Ensure above write is completed */
	readl(base + offset);
}

static inline void qusb2_setbits(void __iomem *base, u32 offset, u32 val)
{
	u32 reg;

	reg = readl(base + offset);
	reg |= val;
	writel(reg, base + offset);

	/* Ensure above write is completed */
	readl(base + offset);
}

static inline void qusb2_clrbits(void __iomem *base, u32 offset, u32 val)
{
	u32 reg;

	reg = readl(base + offset);
	reg &= ~val;
	writel(reg, base + offset);

	/* Ensure above write is completed */
	readl(base + offset);
}

static inline
void qcom_qusb2_phy_configure(void __iomem *base,
			      const unsigned int *regs,
			      const struct qusb2_phy_init_tbl tbl[], int num)
{
	int i;

	for (i = 0; i < num; i++) {
		if (tbl[i].in_layout)
			writel(tbl[i].val, base + regs[tbl[i].offset]);
		else
			writel(tbl[i].val, base + tbl[i].offset);
	}
}

/*
 * Update board specific PHY tuning override values if specified from
 * device tree.
 */
static void qusb2_phy_override_phy_params(struct qusb2_phy *qphy)
{
	const struct qusb2_phy_cfg *cfg = qphy->cfg;
	struct override_params *or = &qphy->overrides;

	if (or->imp_res_offset.override)
		qusb2_write_mask(qphy->base, QUSB2PHY_IMP_CTRL1,
		or->imp_res_offset.value << IMP_RES_OFFSET_SHIFT,
			     IMP_RES_OFFSET_MASK);

	if (or->bias_ctrl.override)
		qusb2_write_mask(qphy->base, QUSB2PHY_PLL_BIAS_CONTROL_2,
		or->bias_ctrl.value << BIAS_CTRL2_RES_OFFSET_SHIFT,
			   BIAS_CTRL2_RES_OFFSET_MASK);

	if (or->charge_ctrl.override)
		qusb2_write_mask(qphy->base, QUSB2PHY_CHG_CTRL2,
		or->charge_ctrl.value << CHG_CTRL2_OFFSET_SHIFT,
			     CHG_CTRL2_OFFSET_MASK);

	if (or->hstx_trim.override)
		qusb2_write_mask(qphy->base, cfg->regs[QUSB2PHY_PORT_TUNE1],
		or->hstx_trim.value << HSTX_TRIM_SHIFT,
				 HSTX_TRIM_MASK);

	if (or->preemphasis.override)
		qusb2_write_mask(qphy->base, cfg->regs[QUSB2PHY_PORT_TUNE1],
		or->preemphasis.value << PREEMPHASIS_EN_SHIFT,
				PREEMPHASIS_EN_MASK);

	if (or->preemphasis_width.override) {
		if (or->preemphasis_width.value ==
		    QUSB2_V2_PREEMPHASIS_WIDTH_HALF_BIT)
			qusb2_setbits(qphy->base,
				      cfg->regs[QUSB2PHY_PORT_TUNE1],
				      PREEMPH_WIDTH_HALF_BIT);
		else
			qusb2_clrbits(qphy->base,
				      cfg->regs[QUSB2PHY_PORT_TUNE1],
				      PREEMPH_WIDTH_HALF_BIT);
	}

	if (or->hsdisc_trim.override)
		qusb2_write_mask(qphy->base, cfg->regs[QUSB2PHY_PORT_TUNE2],
		or->hsdisc_trim.value << HSDISC_TRIM_SHIFT,
				 HSDISC_TRIM_MASK);
}

/*
 * Fetches HS Tx tuning value from nvmem and sets the
 * QUSB2PHY_PORT_TUNE1/2 register.
 * For error case, skip setting the value and use the default value.
 */
static void qusb2_phy_set_tune2_param(struct qusb2_phy *qphy)
{
	struct device *dev = &qphy->phy->dev;
	const struct qusb2_phy_cfg *cfg = qphy->cfg;
	u8 *val, hstx_trim;

	/* efuse register is optional */
	if (!qphy->cell)
		return;

	/*
	 * Read efuse register having TUNE2/1 parameter's high nibble.
	 * If efuse register shows value as 0x0 (indicating value is not
	 * fused), or if we fail to find a valid efuse register setting,
	 * then use default value for high nibble that we have already
	 * set while configuring the phy.
	 */
	val = nvmem_cell_read(qphy->cell, NULL);
	if (IS_ERR(val)) {
		dev_dbg(dev, "failed to read a valid hs-tx trim value\n");
		return;
	}
	hstx_trim = val[0];
	kfree(val);
	if (!hstx_trim) {
		dev_dbg(dev, "failed to read a valid hs-tx trim value\n");
		return;
	}

	/* Fused TUNE1/2 value is the higher nibble only */
	if (cfg->update_tune1_with_efuse)
		qusb2_write_mask(qphy->base, cfg->regs[QUSB2PHY_PORT_TUNE1],
				 hstx_trim << HSTX_TRIM_SHIFT, HSTX_TRIM_MASK);
	else
		qusb2_write_mask(qphy->base, cfg->regs[QUSB2PHY_PORT_TUNE2],
				 hstx_trim << HSTX_TRIM_SHIFT, HSTX_TRIM_MASK);
}

static int qusb2_phy_set_mode(struct phy *phy,
			      enum phy_mode mode, int submode)
{
	struct qusb2_phy *qphy = phy_get_drvdata(phy);

	qphy->mode = mode;

	return 0;
}

static int __maybe_unused qusb2_phy_runtime_suspend(struct device *dev)
{
	struct qusb2_phy *qphy = dev_get_drvdata(dev);
	const struct qusb2_phy_cfg *cfg = qphy->cfg;
	u32 intr_mask;

	dev_vdbg(dev, "Suspending QUSB2 Phy, mode:%d\n", qphy->mode);

	if (!qphy->phy_initialized) {
		dev_vdbg(dev, "PHY not initialized, bailing out\n");
		return 0;
	}

	/*
	 * Enable DP/DM interrupts to detect line state changes based on current
	 * speed. In other words, enable the triggers _opposite_ of what the
	 * current D+/D- levels are e.g. if currently D+ high, D- low
	 * (HS 'J'/Suspend), configure the mask to trigger on D+ low OR D- high
	 */
	intr_mask = DPSE_INTR_EN | DMSE_INTR_EN;
	switch (qphy->mode) {
	case PHY_MODE_USB_HOST_HS:
	case PHY_MODE_USB_HOST_FS:
	case PHY_MODE_USB_DEVICE_HS:
	case PHY_MODE_USB_DEVICE_FS:
		intr_mask |= DMSE_INTR_HIGH_SEL;
		break;
	case PHY_MODE_USB_HOST_LS:
	case PHY_MODE_USB_DEVICE_LS:
		intr_mask |= DPSE_INTR_HIGH_SEL;
		break;
	default:
		/* No device connected, enable both DP/DM high interrupt */
		intr_mask |= DMSE_INTR_HIGH_SEL;
		intr_mask |= DPSE_INTR_HIGH_SEL;
		break;
	}

	writel(intr_mask, qphy->base + cfg->regs[QUSB2PHY_INTR_CTRL]);

	/* hold core PLL into reset */
	if (cfg->has_pll_override) {
		qusb2_setbits(qphy->base,
			      cfg->regs[QUSB2PHY_PLL_CORE_INPUT_OVERRIDE],
			      CORE_PLL_EN_FROM_RESET | CORE_RESET |
			      CORE_RESET_MUX);
	}

	/* enable phy auto-resume only if device is connected on bus */
	if (qphy->mode != PHY_MODE_INVALID) {
		qusb2_setbits(qphy->base, cfg->regs[QUSB2PHY_PORT_TEST1],
			      cfg->autoresume_en);
		/* Autoresume bit has to be toggled in order to enable it */
		qusb2_clrbits(qphy->base, cfg->regs[QUSB2PHY_PORT_TEST1],
			      cfg->autoresume_en);
	}

	if (!qphy->has_se_clk_scheme)
		clk_disable_unprepare(qphy->ref_clk);

	clk_disable_unprepare(qphy->cfg_ahb_clk);
	clk_disable_unprepare(qphy->iface_clk);

	return 0;
}

static int __maybe_unused qusb2_phy_runtime_resume(struct device *dev)
{
	struct qusb2_phy *qphy = dev_get_drvdata(dev);
	const struct qusb2_phy_cfg *cfg = qphy->cfg;
	int ret;

	dev_vdbg(dev, "Resuming QUSB2 phy, mode:%d\n", qphy->mode);

	if (!qphy->phy_initialized) {
		dev_vdbg(dev, "PHY not initialized, bailing out\n");
		return 0;
	}

	ret = clk_prepare_enable(qphy->iface_clk);
	if (ret) {
		dev_err(dev, "failed to enable iface_clk, %d\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(qphy->cfg_ahb_clk);
	if (ret) {
		dev_err(dev, "failed to enable cfg ahb clock, %d\n", ret);
		goto disable_iface_clk;
	}

	if (!qphy->has_se_clk_scheme) {
		ret = clk_prepare_enable(qphy->ref_clk);
		if (ret) {
			dev_err(dev, "failed to enable ref clk, %d\n", ret);
			goto disable_ahb_clk;
		}
	}

	writel(0x0, qphy->base + cfg->regs[QUSB2PHY_INTR_CTRL]);

	/* bring core PLL out of reset */
	if (cfg->has_pll_override) {
		qusb2_clrbits(qphy->base,
			      cfg->regs[QUSB2PHY_PLL_CORE_INPUT_OVERRIDE],
			      CORE_RESET | CORE_RESET_MUX);
	}

	return 0;

disable_ahb_clk:
	clk_disable_unprepare(qphy->cfg_ahb_clk);
disable_iface_clk:
	clk_disable_unprepare(qphy->iface_clk);

	return ret;
}

static int qusb2_phy_init(struct phy *phy)
{
	struct qusb2_phy *qphy = phy_get_drvdata(phy);
	const struct qusb2_phy_cfg *cfg = qphy->cfg;
	unsigned int val = 0;
	unsigned int clk_scheme;
	int ret;

	dev_vdbg(&phy->dev, "%s(): Initializing QUSB2 phy\n", __func__);

	/* Power up regulators and enable clocks (including ref_clk if needed). */
	ret = qusb2_phy_enable_phy(qphy);
	if (ret)
		return ret;

	/* Perform phy reset */
	ret = reset_control_assert(qphy->phy_reset);
	if (ret) {
		dev_err(&phy->dev, "failed to assert phy_reset, %d\n", ret);
		return ret;
	}

	/* 100 us delay to keep PHY in reset mode */
	usleep_range(100, 150);

	ret = reset_control_deassert(qphy->phy_reset);
	if (ret) {
		dev_err(&phy->dev, "failed to de-assert phy_reset, %d\n", ret);
		goto assert_phy_reset;
	}

	/* Disable the PHY */
	qusb2_setbits(qphy->base, cfg->regs[QUSB2PHY_PORT_POWERDOWN],
		      qphy->cfg->disable_ctrl);

	if (cfg->has_pll_test) {
		/* save reset value to override reference clock scheme later */
		val = readl(qphy->base + QUSB2PHY_PLL_TEST);
	}

	qcom_qusb2_phy_configure(qphy->base, cfg->regs, cfg->tbl,
				 cfg->tbl_num);

	/* Override board specific PHY tuning values */
	qusb2_phy_override_phy_params(qphy);

	/* Set efuse value for tuning the PHY */
	qusb2_phy_set_tune2_param(qphy);

	/* Enable the PHY */
	qusb2_clrbits(qphy->base, cfg->regs[QUSB2PHY_PORT_POWERDOWN],
		      POWER_DOWN);

	/* Required to get phy pll lock successfully */
	usleep_range(150, 160);

	/*
	 * Not all the SoCs have got a readable TCSR_PHY_CLK_SCHEME
	 * register in the TCSR so, if there's none, use the default
	 * value hardcoded in the configuration.
	 */
	qphy->has_se_clk_scheme = cfg->se_clk_scheme_default;

	/*
	 * read TCSR_PHY_CLK_SCHEME register to check if single-ended
	 * clock scheme is selected. If yes, then disable differential
	 * ref_clk and use single-ended clock, otherwise use differential
	 * ref_clk only.
	 */
	if (qphy->tcsr) {
		ret = regmap_read(qphy->tcsr, qphy->cfg->clk_scheme_offset,
				  &clk_scheme);
		if (ret) {
			dev_err(&phy->dev, "failed to read clk scheme reg\n");
			goto assert_phy_reset;
		}

		/* is it a differential clock scheme ? */
		if (!(clk_scheme & PHY_CLK_SCHEME_SEL)) {
			dev_vdbg(&phy->dev, "%s(): select differential clk\n",
				 __func__);
			qphy->has_se_clk_scheme = false;
		} else {
			dev_vdbg(&phy->dev, "%s(): select single-ended clk\n",
				 __func__);
		}
	}

	if (!qphy->has_se_clk_scheme) {
		ret = clk_prepare_enable(qphy->ref_clk);
		if (ret) {
			dev_err(&phy->dev, "failed to enable ref clk, %d\n",
				ret);
			goto assert_phy_reset;
		}
	}

	if (cfg->has_pll_test) {
		if (!qphy->has_se_clk_scheme)
			val &= ~CLK_REF_SEL;
		else
			val |= CLK_REF_SEL;

		writel(val, qphy->base + QUSB2PHY_PLL_TEST);

		/* ensure above write is through */
		readl(qphy->base + QUSB2PHY_PLL_TEST);
	}

	/* Required to get phy pll lock successfully */
	usleep_range(100, 110);

	val = readb(qphy->base + cfg->regs[QUSB2PHY_PLL_STATUS]);
	if (!(val & cfg->mask_core_ready)) {
		dev_err(&phy->dev,
			"QUSB2PHY pll lock failed: status reg = %x\n", val);
		ret = -EBUSY;
		goto assert_phy_reset;
	}
	qphy->phy_initialized = true;

	return 0;

assert_phy_reset:
	reset_control_assert(qphy->phy_reset);
	qusb2_phy_disable_phy(qphy);

	return ret;
}

static int qusb2_phy_exit(struct phy *phy)
{
	struct qusb2_phy *qphy = phy_get_drvdata(phy);

	/* Disable the PHY */
	qusb2_setbits(qphy->base, qphy->cfg->regs[QUSB2PHY_PORT_POWERDOWN],
		      qphy->cfg->disable_ctrl);

	if (!qphy->has_se_clk_scheme)
		clk_disable_unprepare(qphy->ref_clk);

	reset_control_assert(qphy->phy_reset);

	qusb2_phy_disable_phy(qphy);

	qphy->phy_initialized = false;

	return 0;
}

/*
 * qusb2_role_switch_set - USB role switch callback.
 *
 * Invoked by the role-switch framework when an upstream user (e.g. a Type-C
 * port manager) requests a role change.  For USB_ROLE_DEVICE we kick off
 * charger detection; the state machine will forward the role to the
 * controller switch only after confirming SDP or CDP.  For USB_ROLE_HOST
 * and USB_ROLE_NONE we forward immediately.
 */
static int qusb2_role_switch_set(struct usb_role_switch *sw, enum usb_role role)
{
	struct qusb2_phy *qphy = usb_role_switch_get_drvdata(sw);

	dev_dbg(&qphy->phy->dev, "role switch set: %s\n", usb_role_string(role));

	mutex_lock(&qphy->role_lock);
	qphy->current_role = role;

	/*
	 * If the controller role switch was not available at probe time (e.g.
	 * the USB controller driver registered its role switch after us), try
	 * to look it up now.  A return of NULL or -ENODEV simply means there
	 * is no controller switch to forward to, which is fine.
	 */
	if (!qphy->controller_role_sw) {
		struct usb_role_switch *dsw = usb_role_switch_get(&qphy->phy->dev);

		if (IS_ERR_OR_NULL(dsw)) {
			if (!IS_ERR(dsw) || PTR_ERR(dsw) == -ENODEV)
				dev_dbg(&qphy->phy->dev,
					"no controller role switch available\n");
			else
				dev_err(&qphy->phy->dev,
					"failed to get controller role switch: %ld\n",
					PTR_ERR(dsw));
		} else {
			qphy->controller_role_sw = dsw;
		}
	}

	switch (role) {
	case USB_ROLE_DEVICE:
		/*
		 * VBUS is active; start charger detection.  The state machine
		 * will forward USB_ROLE_DEVICE controller only for SDP/CDP.
		 */
		qphy->vbus_active = 1;
		qphy->id_state = 1;
		qphy->port_state = PORT_UNKNOWN;
		queue_delayed_work(system_freezable_wq, &qphy->port_det_w, 0);
		break;
	case USB_ROLE_HOST:
		qphy->id_state = 0;
		qphy->vbus_active = 0;
		qphy->port_state = PORT_UNKNOWN;
		queue_delayed_work(system_freezable_wq, &qphy->port_det_w, 0);
		break;
	case USB_ROLE_NONE:
	default:
		qphy->vbus_active = 0;
		qphy->id_state = 1;
		cancel_delayed_work(&qphy->port_det_w);
		qusb2_phy_forward_role(qphy, USB_ROLE_NONE);
		break;
	}
	mutex_unlock(&qphy->role_lock);

	return 0;
}

static const struct phy_ops qusb2_phy_gen_ops = {
	.init		= qusb2_phy_init,
	.exit		= qusb2_phy_exit,
	.set_mode	= qusb2_phy_set_mode,
	.owner		= THIS_MODULE,
};

static const struct of_device_id qusb2_phy_of_match_table[] = {
	{
		.compatible	= "qcom,ipq5424-qusb2-phy",
		.data		= &ipq6018_phy_cfg,
	}, {
		.compatible	= "qcom,ipq6018-qusb2-phy",
		.data		= &ipq6018_phy_cfg,
	}, {
		.compatible	= "qcom,ipq8074-qusb2-phy",
		.data		= &msm8996_phy_cfg,
	}, {
		.compatible	= "qcom,ipq9574-qusb2-phy",
		.data		= &ipq6018_phy_cfg,
	}, {
		.compatible	= "qcom,msm8953-qusb2-phy",
		.data		= &msm8996_phy_cfg,
	}, {
		.compatible	= "qcom,msm8996-qusb2-phy",
		.data		= &msm8996_phy_cfg,
	}, {
		.compatible	= "qcom,msm8998-qusb2-phy",
		.data		= &msm8998_phy_cfg,
	}, {
		.compatible	= "qcom,qcs615-qusb2-phy",
		.data		= &qcs615_phy_cfg,
	}, {
		.compatible	= "qcom,qcm2290-qusb2-phy",
		.data		= &sm6115_phy_cfg,
	}, {
		.compatible	= "qcom,sdm660-qusb2-phy",
		.data		= &sdm660_phy_cfg,
	}, {
		.compatible	= "qcom,sm4250-qusb2-phy",
		.data		= &sm6115_phy_cfg,
	}, {
		.compatible	= "qcom,sm6115-qusb2-phy",
		.data		= &sm6115_phy_cfg,
	}, {
		/*
		 * Deprecated. Only here to support legacy device
		 * trees that didn't include "qcom,qusb2-v2-phy"
		 */
		.compatible	= "qcom,sdm845-qusb2-phy",
		.data		= &qusb2_v2_phy_cfg,
	}, {
		.compatible	= "qcom,qusb2-v2-phy",
		.data		= &qusb2_v2_phy_cfg,
	},
	{ },
};
MODULE_DEVICE_TABLE(of, qusb2_phy_of_match_table);

static const struct dev_pm_ops qusb2_phy_pm_ops = {
	SET_RUNTIME_PM_OPS(qusb2_phy_runtime_suspend,
			   qusb2_phy_runtime_resume, NULL)
};

static int qusb2_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct qusb2_phy *qphy;
	struct phy_provider *phy_provider;
	struct phy *generic_phy;
	int ret, i;
	int num;
	u32 value;
	struct override_params *or;

	qphy = devm_kzalloc(dev, sizeof(*qphy), GFP_KERNEL);
	if (!qphy)
		return -ENOMEM;
	or = &qphy->overrides;

	qphy->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(qphy->base))
		return PTR_ERR(qphy->base);

	qphy->cfg_ahb_clk = devm_clk_get(dev, "cfg_ahb");
	if (IS_ERR(qphy->cfg_ahb_clk))
		return dev_err_probe(dev, PTR_ERR(qphy->cfg_ahb_clk),
				     "failed to get cfg ahb clk\n");

	qphy->ref_clk = devm_clk_get(dev, "ref");
	if (IS_ERR(qphy->ref_clk))
		return dev_err_probe(dev, PTR_ERR(qphy->ref_clk),
				     "failed to get ref clk\n");

	qphy->iface_clk = devm_clk_get_optional(dev, "iface");
	if (IS_ERR(qphy->iface_clk))
		return PTR_ERR(qphy->iface_clk);

	qphy->phy_reset = devm_reset_control_get_by_index(&pdev->dev, 0);
	if (IS_ERR(qphy->phy_reset)) {
		dev_err(dev, "failed to get phy core reset\n");
		return PTR_ERR(qphy->phy_reset);
	}

	num = ARRAY_SIZE(qphy->vregs);
	for (i = 0; i < num; i++)
		qphy->vregs[i].supply = qusb2_phy_vreg_names[i];

	ret = devm_regulator_bulk_get(dev, num, qphy->vregs);
	if (ret)
		return dev_err_probe(dev, ret,
				     "failed to get regulator supplies\n");

	/* Get the specific init parameters of QMP phy */
	qphy->cfg = of_device_get_match_data(dev);

	qphy->tcsr = syscon_regmap_lookup_by_phandle(dev->of_node,
							"qcom,tcsr-syscon");
	if (IS_ERR(qphy->tcsr)) {
		dev_dbg(dev, "failed to lookup TCSR regmap\n");
		qphy->tcsr = NULL;
	}

	qphy->cell = devm_nvmem_cell_get(dev, NULL);
	if (IS_ERR(qphy->cell)) {
		if (PTR_ERR(qphy->cell) == -EPROBE_DEFER)
			return -EPROBE_DEFER;
		qphy->cell = NULL;
		dev_dbg(dev, "failed to lookup tune2 hstx trim value\n");
	}

	if (!of_property_read_u32(dev->of_node, "qcom,imp-res-offset-value",
				  &value)) {
		or->imp_res_offset.value = (u8)value;
		or->imp_res_offset.override = true;
	}

	if (!of_property_read_u32(dev->of_node, "qcom,bias-ctrl-value",
				  &value)) {
		or->bias_ctrl.value = (u8)value;
		or->bias_ctrl.override = true;
	}

	if (!of_property_read_u32(dev->of_node, "qcom,charge-ctrl-value",
				  &value)) {
		or->charge_ctrl.value = (u8)value;
		or->charge_ctrl.override = true;
	}

	if (!of_property_read_u32(dev->of_node, "qcom,hstx-trim-value",
				  &value)) {
		or->hstx_trim.value = (u8)value;
		or->hstx_trim.override = true;
	}

	if (!of_property_read_u32(dev->of_node, "qcom,preemphasis-level",
				     &value)) {
		or->preemphasis.value = (u8)value;
		or->preemphasis.override = true;
	}

	if (!of_property_read_u32(dev->of_node, "qcom,preemphasis-width",
				     &value)) {
		or->preemphasis_width.value = (u8)value;
		or->preemphasis_width.override = true;
	}

	if (!of_property_read_u32(dev->of_node, "qcom,hsdisc-trim-value",
				  &value)) {
		or->hsdisc_trim.value = (u8)value;
		or->hsdisc_trim.override = true;
	}

	dev_set_drvdata(dev, qphy);

	/*
	 * Enable runtime PM support, but forbid it by default.
	 * Users can allow it again via the power/control attribute in sysfs.
	 */
	pm_runtime_set_active(dev);
	pm_runtime_forbid(dev);
	ret = devm_pm_runtime_enable(dev);
	if (ret)
		return ret;

	generic_phy = devm_phy_create(dev, NULL, &qusb2_phy_gen_ops);
	if (IS_ERR(generic_phy)) {
		ret = PTR_ERR(generic_phy);
		dev_err(dev, "failed to create phy, %d\n", ret);
		return ret;
	}
	qphy->phy = generic_phy;

	phy_set_drvdata(generic_phy, qphy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	if (IS_ERR(phy_provider))
		return PTR_ERR(phy_provider);

	mutex_init(&qphy->role_lock);
	INIT_DELAYED_WORK(&qphy->port_det_w, qusb_phy_port_state_work);

	if (device_property_read_bool(dev, "usb-role-switch")) {
		struct usb_role_switch_desc role_sw_desc = {};

		role_sw_desc.fwnode = dev->fwnode;
		role_sw_desc.set = qusb2_role_switch_set;
		role_sw_desc.driver_data = qphy;

		qphy->connector_role_sw = usb_role_switch_register(dev, &role_sw_desc);
		if (IS_ERR(qphy->connector_role_sw)) {
			ret = PTR_ERR(qphy->connector_role_sw);
			dev_err(dev, "failed to register role switch, %d\n", ret);
			return ret;
		}
	}
	qphy->controller_role_sw = NULL;

	return 0;
}

static struct platform_driver qusb2_phy_driver = {
	.probe		= qusb2_phy_probe,
	.driver = {
		.name	= "qcom-qusb2-phy",
		.pm	= &qusb2_phy_pm_ops,
		.of_match_table = qusb2_phy_of_match_table,
	},
};

module_platform_driver(qusb2_phy_driver);

MODULE_AUTHOR("Vivek Gautam <vivek.gautam@codeaurora.org>");
MODULE_DESCRIPTION("Qualcomm QUSB2 PHY driver");
MODULE_LICENSE("GPL v2");
