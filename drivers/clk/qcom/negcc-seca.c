// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,seca-negcc.h>

#include "clk-alpha-pll.h"
#include "clk-branch.h"
#include "clk-pll.h"
#include "clk-rcg.h"
#include "clk-regmap.h"
#include "clk-regmap-divider.h"
#include "clk-regmap-phy-mux.h"
#include "common.h"
#include "gdsc.h"
#include "reset.h"

enum {
	DT_BI_TCXO,
	DT_SLEEP_CLK,
	DT_USB3_PHY_SEC_WRAPPER_NE_GCC_USB31_PIPE_CLK,
	DT_USB3_PHY_WRAPPER_NE_GCC_USB31_PIPE_CLK,
};

enum {
	P_BI_TCXO,
	P_NE_GCC_GPLL0_OUT_EVEN,
	P_NE_GCC_GPLL0_OUT_MAIN,
	P_SLEEP_CLK,
};

static struct clk_alpha_pll ne_gcc_gpll0 = {
	.offset = 0x0,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_OLE],
	.clkr = {
		.enable_reg = 0x57008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_gpll0",
			.parent_data = &(const struct clk_parent_data) {
				.index = DT_BI_TCXO,
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_fixed_lucid_ole_ops,
		},
	},
};

static const struct clk_div_table post_div_table_ne_gcc_gpll0_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv ne_gcc_gpll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 10,
	.post_div_table = post_div_table_ne_gcc_gpll0_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_ne_gcc_gpll0_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_OLE],
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_gpll0_out_even",
		.parent_hws = (const struct clk_hw*[]) {
			&ne_gcc_gpll0.clkr.hw,
		},
		.num_parents = 1,
		.ops = &clk_alpha_pll_postdiv_lucid_ole_ops,
	},
};

static const struct parent_map ne_gcc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_NE_GCC_GPLL0_OUT_MAIN, 1 },
};

static const struct clk_parent_data ne_gcc_parent_data_0[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &ne_gcc_gpll0.clkr.hw },
};

static const struct parent_map ne_gcc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_NE_GCC_GPLL0_OUT_MAIN, 1 },
	{ P_SLEEP_CLK, 5 },
};

static const struct clk_parent_data ne_gcc_parent_data_1[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &ne_gcc_gpll0.clkr.hw },
	{ .index = DT_SLEEP_CLK },
};

static const struct parent_map ne_gcc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
	{ P_NE_GCC_GPLL0_OUT_MAIN, 1 },
	{ P_NE_GCC_GPLL0_OUT_EVEN, 5 },
};

static const struct clk_parent_data ne_gcc_parent_data_2[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &ne_gcc_gpll0.clkr.hw },
	{ .hw = &ne_gcc_gpll0_out_even.clkr.hw },
};

static const struct parent_map ne_gcc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_SLEEP_CLK, 5 },
};

static const struct clk_parent_data ne_gcc_parent_data_3[] = {
	{ .index = DT_BI_TCXO },
	{ .index = DT_SLEEP_CLK },
};

static struct clk_regmap_phy_mux ne_gcc_usb3_prim_phy_pipe_clk_src = {
	.reg = 0x2a078,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb3_prim_phy_pipe_clk_src",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_USB3_PHY_WRAPPER_NE_GCC_USB31_PIPE_CLK,
			},
			.num_parents = 1,
			.ops = &clk_regmap_phy_mux_ops,
		},
	},
};

static struct clk_regmap_phy_mux ne_gcc_usb3_sec_phy_pipe_clk_src = {
	.reg = 0x2c078,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb3_sec_phy_pipe_clk_src",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_USB3_PHY_SEC_WRAPPER_NE_GCC_USB31_PIPE_CLK,
			},
			.num_parents = 1,
			.ops = &clk_regmap_phy_mux_ops,
		},
	},
};

static const struct freq_tbl ftbl_ne_gcc_gp1_clk_src[] = {
	F(66666667, P_NE_GCC_GPLL0_OUT_MAIN, 9, 0, 0),
	F(100000000, P_NE_GCC_GPLL0_OUT_MAIN, 6, 0, 0),
	F(200000000, P_NE_GCC_GPLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 ne_gcc_gp1_clk_src = {
	.cmd_rcgr = 0x21004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_1,
	.freq_tbl = ftbl_ne_gcc_gp1_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_gp1_clk_src",
		.parent_data = ne_gcc_parent_data_1,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 ne_gcc_gp2_clk_src = {
	.cmd_rcgr = 0x22004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_1,
	.freq_tbl = ftbl_ne_gcc_gp1_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_gp2_clk_src",
		.parent_data = ne_gcc_parent_data_1,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static const struct freq_tbl ftbl_ne_gcc_qupv3_wrap2_s0_clk_src[] = {
	F(7372800, P_NE_GCC_GPLL0_OUT_MAIN, 1, 192, 15625),
	F(14745600, P_NE_GCC_GPLL0_OUT_MAIN, 1, 384, 15625),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(29491200, P_NE_GCC_GPLL0_OUT_MAIN, 1, 768, 15625),
	F(32000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 4, 75),
	F(48000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 2, 25),
	F(51200000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 32, 375),
	F(64000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 8, 75),
	F(66666667, P_NE_GCC_GPLL0_OUT_MAIN, 9, 0, 0),
	F(75000000, P_NE_GCC_GPLL0_OUT_MAIN, 8, 0, 0),
	F(80000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 2, 15),
	F(96000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 4, 25),
	F(100000000, P_NE_GCC_GPLL0_OUT_MAIN, 6, 0, 0),
	F(102400000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 64, 375),
	F(112000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 14, 75),
	F(117964800, P_NE_GCC_GPLL0_OUT_MAIN, 1, 3072, 15625),
	F(120000000, P_NE_GCC_GPLL0_OUT_MAIN, 5, 0, 0),
	{ }
};

static struct clk_init_data ne_gcc_qupv3_wrap2_s0_clk_src_init = {
	.name = "ne_gcc_qupv3_wrap2_s0_clk_src",
	.parent_data = ne_gcc_parent_data_0,
	.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 ne_gcc_qupv3_wrap2_s0_clk_src = {
	.cmd_rcgr = 0x3816c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_qupv3_wrap2_s0_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &ne_gcc_qupv3_wrap2_s0_clk_src_init,
};

static struct clk_init_data ne_gcc_qupv3_wrap2_s1_clk_src_init = {
	.name = "ne_gcc_qupv3_wrap2_s1_clk_src",
	.parent_data = ne_gcc_parent_data_0,
	.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 ne_gcc_qupv3_wrap2_s1_clk_src = {
	.cmd_rcgr = 0x382a8,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_qupv3_wrap2_s0_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &ne_gcc_qupv3_wrap2_s1_clk_src_init,
};

static const struct freq_tbl ftbl_ne_gcc_qupv3_wrap2_s2_clk_src[] = {
	F(7372800, P_NE_GCC_GPLL0_OUT_MAIN, 1, 192, 15625),
	F(14745600, P_NE_GCC_GPLL0_OUT_MAIN, 1, 384, 15625),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(29491200, P_NE_GCC_GPLL0_OUT_MAIN, 1, 768, 15625),
	F(32000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 4, 75),
	F(48000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 2, 25),
	F(51200000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 32, 375),
	F(64000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 8, 75),
	F(66666667, P_NE_GCC_GPLL0_OUT_MAIN, 9, 0, 0),
	F(75000000, P_NE_GCC_GPLL0_OUT_MAIN, 8, 0, 0),
	F(80000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 2, 15),
	F(96000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 4, 25),
	F(100000000, P_NE_GCC_GPLL0_OUT_MAIN, 6, 0, 0),
	{ }
};

static struct clk_init_data ne_gcc_qupv3_wrap2_s2_clk_src_init = {
	.name = "ne_gcc_qupv3_wrap2_s2_clk_src",
	.parent_data = ne_gcc_parent_data_0,
	.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 ne_gcc_qupv3_wrap2_s2_clk_src = {
	.cmd_rcgr = 0x383e4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_qupv3_wrap2_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &ne_gcc_qupv3_wrap2_s2_clk_src_init,
};

static struct clk_init_data ne_gcc_qupv3_wrap2_s3_clk_src_init = {
	.name = "ne_gcc_qupv3_wrap2_s3_clk_src",
	.parent_data = ne_gcc_parent_data_0,
	.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 ne_gcc_qupv3_wrap2_s3_clk_src = {
	.cmd_rcgr = 0x38520,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_qupv3_wrap2_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &ne_gcc_qupv3_wrap2_s3_clk_src_init,
};

static struct clk_init_data ne_gcc_qupv3_wrap2_s4_clk_src_init = {
	.name = "ne_gcc_qupv3_wrap2_s4_clk_src",
	.parent_data = ne_gcc_parent_data_0,
	.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 ne_gcc_qupv3_wrap2_s4_clk_src = {
	.cmd_rcgr = 0x3865c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_qupv3_wrap2_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &ne_gcc_qupv3_wrap2_s4_clk_src_init,
};

static struct clk_init_data ne_gcc_qupv3_wrap2_s5_clk_src_init = {
	.name = "ne_gcc_qupv3_wrap2_s5_clk_src",
	.parent_data = ne_gcc_parent_data_0,
	.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 ne_gcc_qupv3_wrap2_s5_clk_src = {
	.cmd_rcgr = 0x38798,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_qupv3_wrap2_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &ne_gcc_qupv3_wrap2_s5_clk_src_init,
};

static struct clk_init_data ne_gcc_qupv3_wrap2_s6_clk_src_init = {
	.name = "ne_gcc_qupv3_wrap2_s6_clk_src",
	.parent_data = ne_gcc_parent_data_0,
	.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 ne_gcc_qupv3_wrap2_s6_clk_src = {
	.cmd_rcgr = 0x388d4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_qupv3_wrap2_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &ne_gcc_qupv3_wrap2_s6_clk_src_init,
};

static const struct freq_tbl ftbl_ne_gcc_sdcc4_apps_clk_src[] = {
	F(144000, P_BI_TCXO, 16, 3, 25),
	F(400000, P_BI_TCXO, 12, 1, 4),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(20000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 1, 30),
	F(25000000, P_NE_GCC_GPLL0_OUT_MAIN, 1, 1, 24),
	F(37500000, P_NE_GCC_GPLL0_OUT_MAIN, 16, 0, 0),
	F(50000000, P_NE_GCC_GPLL0_OUT_MAIN, 12, 0, 0),
	F(100000000, P_NE_GCC_GPLL0_OUT_MAIN, 6, 0, 0),
	{ }
};

static struct clk_rcg2 ne_gcc_sdcc4_apps_clk_src = {
	.cmd_rcgr = 0x1801c,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_sdcc4_apps_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_sdcc4_apps_clk_src",
		.parent_data = ne_gcc_parent_data_0,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_floor_ops,
	},
};

static const struct freq_tbl ftbl_ne_gcc_usb20_master_clk_src[] = {
	F(75000000, P_NE_GCC_GPLL0_OUT_MAIN, 8, 0, 0),
	F(120000000, P_NE_GCC_GPLL0_OUT_MAIN, 5, 0, 0),
	{ }
};

static struct clk_rcg2 ne_gcc_usb20_master_clk_src = {
	.cmd_rcgr = 0x31030,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_usb20_master_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb20_master_clk_src",
		.parent_data = ne_gcc_parent_data_0,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static const struct freq_tbl ftbl_ne_gcc_usb20_mock_utmi_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 ne_gcc_usb20_mock_utmi_clk_src = {
	.cmd_rcgr = 0x31048,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_usb20_mock_utmi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb20_mock_utmi_clk_src",
		.parent_data = ne_gcc_parent_data_0,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static const struct freq_tbl ftbl_ne_gcc_usb31_prim_master_clk_src[] = {
	F(85714286, P_NE_GCC_GPLL0_OUT_MAIN, 7, 0, 0),
	F(133333333, P_NE_GCC_GPLL0_OUT_MAIN, 4.5, 0, 0),
	F(200000000, P_NE_GCC_GPLL0_OUT_MAIN, 3, 0, 0),
	F(240000000, P_NE_GCC_GPLL0_OUT_MAIN, 2.5, 0, 0),
	{ }
};

static struct clk_rcg2 ne_gcc_usb31_prim_master_clk_src = {
	.cmd_rcgr = 0x2a038,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_2,
	.freq_tbl = ftbl_ne_gcc_usb31_prim_master_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb31_prim_master_clk_src",
		.parent_data = ne_gcc_parent_data_2,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 ne_gcc_usb31_prim_mock_utmi_clk_src = {
	.cmd_rcgr = 0x2a050,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_usb20_mock_utmi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb31_prim_mock_utmi_clk_src",
		.parent_data = ne_gcc_parent_data_0,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 ne_gcc_usb31_sec_master_clk_src = {
	.cmd_rcgr = 0x2c038,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_usb31_prim_master_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb31_sec_master_clk_src",
		.parent_data = ne_gcc_parent_data_0,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 ne_gcc_usb31_sec_mock_utmi_clk_src = {
	.cmd_rcgr = 0x2c050,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_0,
	.freq_tbl = ftbl_ne_gcc_usb20_mock_utmi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb31_sec_mock_utmi_clk_src",
		.parent_data = ne_gcc_parent_data_0,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 ne_gcc_usb3_prim_phy_aux_clk_src = {
	.cmd_rcgr = 0x2a07c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_3,
	.freq_tbl = ftbl_ne_gcc_usb20_mock_utmi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb3_prim_phy_aux_clk_src",
		.parent_data = ne_gcc_parent_data_3,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_3),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 ne_gcc_usb3_sec_phy_aux_clk_src = {
	.cmd_rcgr = 0x2c07c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = ne_gcc_parent_map_3,
	.freq_tbl = ftbl_ne_gcc_usb20_mock_utmi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb3_sec_phy_aux_clk_src",
		.parent_data = ne_gcc_parent_data_3,
		.num_parents = ARRAY_SIZE(ne_gcc_parent_data_3),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_regmap_div ne_gcc_usb20_mock_utmi_postdiv_clk_src = {
	.reg = 0x31060,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb20_mock_utmi_postdiv_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&ne_gcc_usb20_mock_utmi_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div ne_gcc_usb31_prim_mock_utmi_postdiv_clk_src = {
	.reg = 0x2a068,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb31_prim_mock_utmi_postdiv_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&ne_gcc_usb31_prim_mock_utmi_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_regmap_div ne_gcc_usb31_sec_mock_utmi_postdiv_clk_src = {
	.reg = 0x2c068,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "ne_gcc_usb31_sec_mock_utmi_postdiv_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&ne_gcc_usb31_sec_mock_utmi_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch ne_gcc_aggre_noc_usb2_axi_clk = {
	.halt_reg = 0x31068,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x31068,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x31068,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_aggre_noc_usb2_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb20_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_aggre_noc_usb3_prim_axi_clk = {
	.halt_reg = 0x2a098,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x2a098,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x2a098,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_aggre_noc_usb3_prim_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb31_prim_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_aggre_noc_usb3_sec_axi_clk = {
	.halt_reg = 0x2c098,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x2c098,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x2c098,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_aggre_noc_usb3_sec_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb31_sec_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_cnoc_usb2_axi_clk = {
	.halt_reg = 0x31064,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x31064,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x31064,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_cnoc_usb2_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb20_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_cnoc_usb3_prim_axi_clk = {
	.halt_reg = 0x2a094,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x2a094,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x2a094,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_cnoc_usb3_prim_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb31_prim_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_cnoc_usb3_sec_axi_clk = {
	.halt_reg = 0x2c094,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x2c094,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x2c094,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_cnoc_usb3_sec_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb31_sec_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_frq_measure_ref_clk = {
	.halt_reg = 0x20008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x20008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_frq_measure_ref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_gp1_clk = {
	.halt_reg = 0x21000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_gp1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_gp1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_gp2_clk = {
	.halt_reg = 0x22000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x22000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_gp2_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_gp2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_core_2x_clk = {
	.halt_reg = 0x38020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(24),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_core_2x_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_core_clk = {
	.halt_reg = 0x3800c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(23),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_core_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_m_ahb_clk = {
	.halt_reg = 0x38004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x38004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(21),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_m_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_s0_clk = {
	.halt_reg = 0x3815c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(25),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_s0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_qupv3_wrap2_s0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_s1_clk = {
	.halt_reg = 0x38298,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(26),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_s1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_qupv3_wrap2_s1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_s2_clk = {
	.halt_reg = 0x383d4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(27),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_s2_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_qupv3_wrap2_s2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_s3_clk = {
	.halt_reg = 0x38510,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(28),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_s3_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_qupv3_wrap2_s3_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_s4_clk = {
	.halt_reg = 0x3864c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(29),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_s4_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_qupv3_wrap2_s4_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_s5_clk = {
	.halt_reg = 0x38788,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(30),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_s5_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_qupv3_wrap2_s5_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_s6_clk = {
	.halt_reg = 0x388c4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(31),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_s6_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_qupv3_wrap2_s6_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_qupv3_wrap2_s_ahb_clk = {
	.halt_reg = 0x38008,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x38008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x57000,
		.enable_mask = BIT(22),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_qupv3_wrap2_s_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_sdcc4_apps_clk = {
	.halt_reg = 0x18004,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x18004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_sdcc4_apps_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_sdcc4_apps_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_sdcc4_axi_clk = {
	.halt_reg = 0x18014,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x18014,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_sdcc4_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb20_master_clk = {
	.halt_reg = 0x31018,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x31018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb20_master_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb20_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb20_mock_utmi_clk = {
	.halt_reg = 0x3102c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x3102c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb20_mock_utmi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb20_mock_utmi_postdiv_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb20_sleep_clk = {
	.halt_reg = 0x31028,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x31028,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb20_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb31_prim_eud_ahb_clk = {
	.halt_reg = 0x2a02c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x2a02c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x2a02c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb31_prim_eud_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb31_prim_master_clk = {
	.halt_reg = 0x2a01c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2a01c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb31_prim_master_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb31_prim_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb31_prim_mock_utmi_clk = {
	.halt_reg = 0x2a034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2a034,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb31_prim_mock_utmi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb31_prim_mock_utmi_postdiv_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb31_prim_sleep_clk = {
	.halt_reg = 0x2a030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2a030,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb31_prim_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb31_sec_eud_ahb_clk = {
	.halt_reg = 0x2c02c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x2c02c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x2c02c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb31_sec_eud_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb31_sec_master_clk = {
	.halt_reg = 0x2c01c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2c01c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb31_sec_master_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb31_sec_master_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb31_sec_mock_utmi_clk = {
	.halt_reg = 0x2c034,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2c034,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb31_sec_mock_utmi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb31_sec_mock_utmi_postdiv_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb31_sec_sleep_clk = {
	.halt_reg = 0x2c030,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2c030,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb31_sec_sleep_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb3_prim_phy_aux_clk = {
	.halt_reg = 0x2a06c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2a06c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb3_prim_phy_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb3_prim_phy_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb3_prim_phy_com_aux_clk = {
	.halt_reg = 0x2a070,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2a070,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb3_prim_phy_com_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb3_prim_phy_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb3_prim_phy_pipe_clk = {
	.halt_reg = 0x2a074,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x2a074,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x2a074,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb3_prim_phy_pipe_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb3_prim_phy_pipe_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb3_sec_phy_aux_clk = {
	.halt_reg = 0x2c06c,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2c06c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb3_sec_phy_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb3_sec_phy_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb3_sec_phy_com_aux_clk = {
	.halt_reg = 0x2c070,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x2c070,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb3_sec_phy_com_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb3_sec_phy_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch ne_gcc_usb3_sec_phy_pipe_clk = {
	.halt_reg = 0x2c074,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x2c074,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x2c074,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "ne_gcc_usb3_sec_phy_pipe_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&ne_gcc_usb3_sec_phy_pipe_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct gdsc ne_gcc_usb31_prim_gdsc = {
	.gdscr = 0x2a004,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "ne_gcc_usb31_prim_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct gdsc ne_gcc_usb3_phy_gdsc = {
	.gdscr = 0x2b00c,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x2,
	.pd = {
		.name = "ne_gcc_usb3_phy_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct gdsc ne_gcc_usb31_sec_gdsc = {
	.gdscr = 0x2c004,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "ne_gcc_usb31_sec_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct gdsc ne_gcc_usb20_prim_gdsc = {
	.gdscr = 0x31004,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "ne_gcc_usb20_prim_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct gdsc ne_gcc_usb3_sec_phy_gdsc = {
	.gdscr = 0x2d00c,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x2,
	.pd = {
		.name = "ne_gcc_usb3_sec_phy_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct clk_regmap *ne_gcc_seca_clocks[] = {
	[NE_GCC_AGGRE_NOC_USB2_AXI_CLK] = &ne_gcc_aggre_noc_usb2_axi_clk.clkr,
	[NE_GCC_AGGRE_NOC_USB3_PRIM_AXI_CLK] = &ne_gcc_aggre_noc_usb3_prim_axi_clk.clkr,
	[NE_GCC_AGGRE_NOC_USB3_SEC_AXI_CLK] = &ne_gcc_aggre_noc_usb3_sec_axi_clk.clkr,
	[NE_GCC_CNOC_USB2_AXI_CLK] = &ne_gcc_cnoc_usb2_axi_clk.clkr,
	[NE_GCC_CNOC_USB3_PRIM_AXI_CLK] = &ne_gcc_cnoc_usb3_prim_axi_clk.clkr,
	[NE_GCC_CNOC_USB3_SEC_AXI_CLK] = &ne_gcc_cnoc_usb3_sec_axi_clk.clkr,
	[NE_GCC_FRQ_MEASURE_REF_CLK] = &ne_gcc_frq_measure_ref_clk.clkr,
	[NE_GCC_GP1_CLK] = &ne_gcc_gp1_clk.clkr,
	[NE_GCC_GP1_CLK_SRC] = &ne_gcc_gp1_clk_src.clkr,
	[NE_GCC_GP2_CLK] = &ne_gcc_gp2_clk.clkr,
	[NE_GCC_GP2_CLK_SRC] = &ne_gcc_gp2_clk_src.clkr,
	[NE_GCC_GPLL0] = &ne_gcc_gpll0.clkr,
	[NE_GCC_GPLL0_OUT_EVEN] = &ne_gcc_gpll0_out_even.clkr,
	[NE_GCC_QUPV3_WRAP2_CORE_2X_CLK] = &ne_gcc_qupv3_wrap2_core_2x_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_CORE_CLK] = &ne_gcc_qupv3_wrap2_core_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_M_AHB_CLK] = &ne_gcc_qupv3_wrap2_m_ahb_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_S0_CLK] = &ne_gcc_qupv3_wrap2_s0_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_S0_CLK_SRC] = &ne_gcc_qupv3_wrap2_s0_clk_src.clkr,
	[NE_GCC_QUPV3_WRAP2_S1_CLK] = &ne_gcc_qupv3_wrap2_s1_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_S1_CLK_SRC] = &ne_gcc_qupv3_wrap2_s1_clk_src.clkr,
	[NE_GCC_QUPV3_WRAP2_S2_CLK] = &ne_gcc_qupv3_wrap2_s2_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_S2_CLK_SRC] = &ne_gcc_qupv3_wrap2_s2_clk_src.clkr,
	[NE_GCC_QUPV3_WRAP2_S3_CLK] = &ne_gcc_qupv3_wrap2_s3_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_S3_CLK_SRC] = &ne_gcc_qupv3_wrap2_s3_clk_src.clkr,
	[NE_GCC_QUPV3_WRAP2_S4_CLK] = &ne_gcc_qupv3_wrap2_s4_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_S4_CLK_SRC] = &ne_gcc_qupv3_wrap2_s4_clk_src.clkr,
	[NE_GCC_QUPV3_WRAP2_S5_CLK] = &ne_gcc_qupv3_wrap2_s5_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_S5_CLK_SRC] = &ne_gcc_qupv3_wrap2_s5_clk_src.clkr,
	[NE_GCC_QUPV3_WRAP2_S6_CLK] = &ne_gcc_qupv3_wrap2_s6_clk.clkr,
	[NE_GCC_QUPV3_WRAP2_S6_CLK_SRC] = &ne_gcc_qupv3_wrap2_s6_clk_src.clkr,
	[NE_GCC_QUPV3_WRAP2_S_AHB_CLK] = &ne_gcc_qupv3_wrap2_s_ahb_clk.clkr,
	[NE_GCC_SDCC4_APPS_CLK] = &ne_gcc_sdcc4_apps_clk.clkr,
	[NE_GCC_SDCC4_APPS_CLK_SRC] = &ne_gcc_sdcc4_apps_clk_src.clkr,
	[NE_GCC_SDCC4_AXI_CLK] = &ne_gcc_sdcc4_axi_clk.clkr,
	[NE_GCC_USB20_MASTER_CLK] = &ne_gcc_usb20_master_clk.clkr,
	[NE_GCC_USB20_MASTER_CLK_SRC] = &ne_gcc_usb20_master_clk_src.clkr,
	[NE_GCC_USB20_MOCK_UTMI_CLK] = &ne_gcc_usb20_mock_utmi_clk.clkr,
	[NE_GCC_USB20_MOCK_UTMI_CLK_SRC] = &ne_gcc_usb20_mock_utmi_clk_src.clkr,
	[NE_GCC_USB20_MOCK_UTMI_POSTDIV_CLK_SRC] = &ne_gcc_usb20_mock_utmi_postdiv_clk_src.clkr,
	[NE_GCC_USB20_SLEEP_CLK] = &ne_gcc_usb20_sleep_clk.clkr,
	[NE_GCC_USB31_PRIM_EUD_AHB_CLK] = &ne_gcc_usb31_prim_eud_ahb_clk.clkr,
	[NE_GCC_USB31_PRIM_MASTER_CLK] = &ne_gcc_usb31_prim_master_clk.clkr,
	[NE_GCC_USB31_PRIM_MASTER_CLK_SRC] = &ne_gcc_usb31_prim_master_clk_src.clkr,
	[NE_GCC_USB31_PRIM_MOCK_UTMI_CLK] = &ne_gcc_usb31_prim_mock_utmi_clk.clkr,
	[NE_GCC_USB31_PRIM_MOCK_UTMI_CLK_SRC] = &ne_gcc_usb31_prim_mock_utmi_clk_src.clkr,
	[NE_GCC_USB31_PRIM_MOCK_UTMI_POSTDIV_CLK_SRC] =
		&ne_gcc_usb31_prim_mock_utmi_postdiv_clk_src.clkr,
	[NE_GCC_USB31_PRIM_SLEEP_CLK] = &ne_gcc_usb31_prim_sleep_clk.clkr,
	[NE_GCC_USB31_SEC_EUD_AHB_CLK] = &ne_gcc_usb31_sec_eud_ahb_clk.clkr,
	[NE_GCC_USB31_SEC_MASTER_CLK] = &ne_gcc_usb31_sec_master_clk.clkr,
	[NE_GCC_USB31_SEC_MASTER_CLK_SRC] = &ne_gcc_usb31_sec_master_clk_src.clkr,
	[NE_GCC_USB31_SEC_MOCK_UTMI_CLK] = &ne_gcc_usb31_sec_mock_utmi_clk.clkr,
	[NE_GCC_USB31_SEC_MOCK_UTMI_CLK_SRC] = &ne_gcc_usb31_sec_mock_utmi_clk_src.clkr,
	[NE_GCC_USB31_SEC_MOCK_UTMI_POSTDIV_CLK_SRC] =
		&ne_gcc_usb31_sec_mock_utmi_postdiv_clk_src.clkr,
	[NE_GCC_USB31_SEC_SLEEP_CLK] = &ne_gcc_usb31_sec_sleep_clk.clkr,
	[NE_GCC_USB3_PRIM_PHY_AUX_CLK] = &ne_gcc_usb3_prim_phy_aux_clk.clkr,
	[NE_GCC_USB3_PRIM_PHY_AUX_CLK_SRC] = &ne_gcc_usb3_prim_phy_aux_clk_src.clkr,
	[NE_GCC_USB3_PRIM_PHY_COM_AUX_CLK] = &ne_gcc_usb3_prim_phy_com_aux_clk.clkr,
	[NE_GCC_USB3_PRIM_PHY_PIPE_CLK] = &ne_gcc_usb3_prim_phy_pipe_clk.clkr,
	[NE_GCC_USB3_PRIM_PHY_PIPE_CLK_SRC] = &ne_gcc_usb3_prim_phy_pipe_clk_src.clkr,
	[NE_GCC_USB3_SEC_PHY_AUX_CLK] = &ne_gcc_usb3_sec_phy_aux_clk.clkr,
	[NE_GCC_USB3_SEC_PHY_AUX_CLK_SRC] = &ne_gcc_usb3_sec_phy_aux_clk_src.clkr,
	[NE_GCC_USB3_SEC_PHY_COM_AUX_CLK] = &ne_gcc_usb3_sec_phy_com_aux_clk.clkr,
	[NE_GCC_USB3_SEC_PHY_PIPE_CLK] = &ne_gcc_usb3_sec_phy_pipe_clk.clkr,
	[NE_GCC_USB3_SEC_PHY_PIPE_CLK_SRC] = &ne_gcc_usb3_sec_phy_pipe_clk_src.clkr,
};

static struct gdsc *ne_gcc_seca_gdscs[] = {
	[NE_GCC_USB20_PRIM_GDSC] = &ne_gcc_usb20_prim_gdsc,
	[NE_GCC_USB31_PRIM_GDSC] = &ne_gcc_usb31_prim_gdsc,
	[NE_GCC_USB31_SEC_GDSC] = &ne_gcc_usb31_sec_gdsc,
	[NE_GCC_USB3_PHY_GDSC] = &ne_gcc_usb3_phy_gdsc,
	[NE_GCC_USB3_SEC_PHY_GDSC] = &ne_gcc_usb3_sec_phy_gdsc,
};

static const struct qcom_reset_map ne_gcc_seca_resets[] = {
	[NE_GCC_QUPV3_WRAPPER_2_BCR] = { 0x38000 },
	[NE_GCC_QUSB2PHY_PRIM_BCR] = { 0x2e000 },
	[NE_GCC_QUSB2PHY_SEC_BCR] = { 0x2f000 },
	[NE_GCC_SDCC4_BCR] = { 0x18000 },
	[NE_GCC_USB20_PRIM_BCR] = { 0x31000 },
	[NE_GCC_USB31_PRIM_BCR] = { 0x2a000 },
	[NE_GCC_USB31_SEC_BCR] = { 0x2c000 },
	[NE_GCC_USB3_DP_PHY_PRIM_BCR] = { 0x2b008 },
	[NE_GCC_USB3_DP_PHY_SEC_BCR] = { 0x2d008 },
	[NE_GCC_USB3_PHY_PRIM_BCR] = { 0x2b000 },
	[NE_GCC_USB3_PHY_SEC_BCR] = { 0x2d000 },
	[NE_GCC_USB3PHY_PHY_PRIM_BCR] = { 0x2b004 },
	[NE_GCC_USB3PHY_PHY_SEC_BCR] = { 0x2d004 },
};

static const struct clk_rcg_dfs_data ne_gcc_seca_dfs_clocks[] = {
	DEFINE_RCG_DFS(ne_gcc_qupv3_wrap2_s0_clk_src),
	DEFINE_RCG_DFS(ne_gcc_qupv3_wrap2_s1_clk_src),
	DEFINE_RCG_DFS(ne_gcc_qupv3_wrap2_s2_clk_src),
	DEFINE_RCG_DFS(ne_gcc_qupv3_wrap2_s3_clk_src),
	DEFINE_RCG_DFS(ne_gcc_qupv3_wrap2_s4_clk_src),
	DEFINE_RCG_DFS(ne_gcc_qupv3_wrap2_s5_clk_src),
	DEFINE_RCG_DFS(ne_gcc_qupv3_wrap2_s6_clk_src),
};

static const struct regmap_config ne_gcc_seca_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xf41f0,
	.fast_io = true,
};

static const u32 ne_gcc_seca_critical_cbcrs[] = {
	0x30004, /* NE_GCC_AHB2PHY_CLK */
};

static const struct qcom_cc_driver_data ne_gcc_seca_driver_data = {
	.clk_cbcrs = ne_gcc_seca_critical_cbcrs,
	.num_clk_cbcrs = ARRAY_SIZE(ne_gcc_seca_critical_cbcrs),
	.dfs_rcgs = ne_gcc_seca_dfs_clocks,
	.num_dfs_rcgs = ARRAY_SIZE(ne_gcc_seca_dfs_clocks),
};

static const struct qcom_cc_desc ne_gcc_seca_desc = {
	.config = &ne_gcc_seca_regmap_config,
	.clks = ne_gcc_seca_clocks,
	.num_clks = ARRAY_SIZE(ne_gcc_seca_clocks),
	.resets = ne_gcc_seca_resets,
	.num_resets = ARRAY_SIZE(ne_gcc_seca_resets),
	.gdscs = ne_gcc_seca_gdscs,
	.num_gdscs = ARRAY_SIZE(ne_gcc_seca_gdscs),
	.driver_data = &ne_gcc_seca_driver_data,
};

static const struct of_device_id ne_gcc_seca_match_table[] = {
	{ .compatible = "qcom,seca-negcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, ne_gcc_seca_match_table);

static int ne_gcc_seca_probe(struct platform_device *pdev)
{
	return qcom_cc_probe(pdev, &ne_gcc_seca_desc);
}

static struct platform_driver ne_gcc_seca_driver = {
	.probe = ne_gcc_seca_probe,
	.driver = {
		.name = "negcc-seca",
		.of_match_table = ne_gcc_seca_match_table,
	},
};

module_platform_driver(ne_gcc_seca_driver);

MODULE_DESCRIPTION("QTI NEGCC Seca driver");
MODULE_LICENSE("GPL");
