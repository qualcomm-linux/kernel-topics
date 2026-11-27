// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/clk-provider.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include <dt-bindings/clock/qcom,seca-nwgcc.h>

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
	DT_UFS_0_PHY_RX_SYMBOL_0_CLK,
	DT_UFS_0_PHY_RX_SYMBOL_1_CLK,
	DT_UFS_0_PHY_TX_SYMBOL_0_CLK,
	DT_UFS_1_PHY_RX_SYMBOL_0_CLK,
	DT_UFS_1_PHY_RX_SYMBOL_1_CLK,
	DT_UFS_1_PHY_TX_SYMBOL_0_CLK,
};

enum {
	P_BI_TCXO,
	P_NW_GCC_GPLL0_OUT_EVEN,
	P_NW_GCC_GPLL0_OUT_MAIN,
	P_NW_GCC_GPLL3_OUT_MAIN,
	P_SLEEP_CLK,
};

static struct clk_alpha_pll nw_gcc_gpll0 = {
	.offset = 0x0,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_OLE],
	.clkr = {
		.enable_reg = 0x76010,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_gpll0",
			.parent_data = &(const struct clk_parent_data) {
				.index = DT_BI_TCXO,
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_fixed_lucid_ole_ops,
		},
	},
};

static const struct clk_div_table post_div_table_nw_gcc_gpll0_out_even[] = {
	{ 0x1, 2 },
	{ }
};

static struct clk_alpha_pll_postdiv nw_gcc_gpll0_out_even = {
	.offset = 0x0,
	.post_div_shift = 10,
	.post_div_table = post_div_table_nw_gcc_gpll0_out_even,
	.num_post_div = ARRAY_SIZE(post_div_table_nw_gcc_gpll0_out_even),
	.width = 4,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_OLE],
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_gpll0_out_even",
		.parent_hws = (const struct clk_hw*[]) {
			&nw_gcc_gpll0.clkr.hw,
		},
		.num_parents = 1,
		.ops = &clk_alpha_pll_postdiv_lucid_ole_ops,
	},
};

static struct clk_alpha_pll nw_gcc_gpll3 = {
	.offset = 0x3000,
	.regs = clk_alpha_pll_regs[CLK_ALPHA_PLL_TYPE_LUCID_OLE],
	.clkr = {
		.enable_reg = 0x76010,
		.enable_mask = BIT(3),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_gpll3",
			.parent_data = &(const struct clk_parent_data) {
				.index = DT_BI_TCXO,
			},
			.num_parents = 1,
			.ops = &clk_alpha_pll_fixed_lucid_ole_ops,
		},
	},
};

static const struct parent_map nw_gcc_parent_map_0[] = {
	{ P_BI_TCXO, 0 },
	{ P_NW_GCC_GPLL0_OUT_MAIN, 1 },
	{ P_SLEEP_CLK, 5 },
	{ P_NW_GCC_GPLL0_OUT_EVEN, 6 },
};

static const struct clk_parent_data nw_gcc_parent_data_0[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &nw_gcc_gpll0.clkr.hw },
	{ .index = DT_SLEEP_CLK },
	{ .hw = &nw_gcc_gpll0_out_even.clkr.hw },
};

static const struct parent_map nw_gcc_parent_map_1[] = {
	{ P_BI_TCXO, 0 },
	{ P_NW_GCC_GPLL0_OUT_MAIN, 1 },
	{ P_NW_GCC_GPLL3_OUT_MAIN, 3 },
};

static const struct clk_parent_data nw_gcc_parent_data_1[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &nw_gcc_gpll0.clkr.hw },
	{ .hw = &nw_gcc_gpll3.clkr.hw },
};

static const struct parent_map nw_gcc_parent_map_2[] = {
	{ P_BI_TCXO, 0 },
};

static const struct clk_parent_data nw_gcc_parent_data_2[] = {
	{ .index = DT_BI_TCXO },
};

static const struct parent_map nw_gcc_parent_map_3[] = {
	{ P_BI_TCXO, 0 },
	{ P_NW_GCC_GPLL0_OUT_MAIN, 1 },
	{ P_NW_GCC_GPLL3_OUT_MAIN, 5 },
};

static const struct parent_map nw_gcc_parent_map_4[] = {
	{ P_BI_TCXO, 0 },
	{ P_NW_GCC_GPLL0_OUT_MAIN, 1 },
};

static const struct clk_parent_data nw_gcc_parent_data_4[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &nw_gcc_gpll0.clkr.hw },
};

static const struct parent_map nw_gcc_parent_map_5[] = {
	{ P_BI_TCXO, 0 },
	{ P_NW_GCC_GPLL0_OUT_MAIN, 1 },
	{ P_NW_GCC_GPLL0_OUT_EVEN, 2 },
};

static const struct clk_parent_data nw_gcc_parent_data_5[] = {
	{ .index = DT_BI_TCXO },
	{ .hw = &nw_gcc_gpll0.clkr.hw },
	{ .hw = &nw_gcc_gpll0_out_even.clkr.hw },
};

static struct clk_regmap_phy_mux nw_gcc_ufs_0_phy_rx_symbol_0_clk_src = {
	.reg = 0x52068,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_rx_symbol_0_clk_src",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_UFS_0_PHY_RX_SYMBOL_0_CLK,
			},
			.num_parents = 1,
			.ops = &clk_regmap_phy_mux_ops,
		},
	},
};

static struct clk_regmap_phy_mux nw_gcc_ufs_0_phy_rx_symbol_1_clk_src = {
	.reg = 0x520f0,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_rx_symbol_1_clk_src",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_UFS_0_PHY_RX_SYMBOL_1_CLK,
			},
			.num_parents = 1,
			.ops = &clk_regmap_phy_mux_ops,
		},
	},
};

static struct clk_regmap_phy_mux nw_gcc_ufs_0_phy_tx_symbol_0_clk_src = {
	.reg = 0x52058,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_tx_symbol_0_clk_src",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_UFS_0_PHY_TX_SYMBOL_0_CLK,
			},
			.num_parents = 1,
			.ops = &clk_regmap_phy_mux_ops,
		},
	},
};

static struct clk_regmap_phy_mux nw_gcc_ufs_1_phy_rx_symbol_0_clk_src = {
	.reg = 0x3d068,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_rx_symbol_0_clk_src",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_UFS_1_PHY_RX_SYMBOL_0_CLK,
			},
			.num_parents = 1,
			.ops = &clk_regmap_phy_mux_ops,
		},
	},
};

static struct clk_regmap_phy_mux nw_gcc_ufs_1_phy_rx_symbol_1_clk_src = {
	.reg = 0x3d0f0,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_rx_symbol_1_clk_src",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_UFS_1_PHY_RX_SYMBOL_1_CLK,
			},
			.num_parents = 1,
			.ops = &clk_regmap_phy_mux_ops,
		},
	},
};

static struct clk_regmap_phy_mux nw_gcc_ufs_1_phy_tx_symbol_0_clk_src = {
	.reg = 0x3d058,
	.clkr = {
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_tx_symbol_0_clk_src",
			.parent_data = &(const struct clk_parent_data){
				.index = DT_UFS_1_PHY_TX_SYMBOL_0_CLK,
			},
			.num_parents = 1,
			.ops = &clk_regmap_phy_mux_ops,
		},
	},
};

static const struct freq_tbl ftbl_nw_gcc_gp1_clk_src[] = {
	F(60000000, P_NW_GCC_GPLL0_OUT_MAIN, 10, 0, 0),
	F(100000000, P_NW_GCC_GPLL0_OUT_MAIN, 6, 0, 0),
	F(200000000, P_NW_GCC_GPLL0_OUT_MAIN, 3, 0, 0),
	{ }
};

static struct clk_rcg2 nw_gcc_gp1_clk_src = {
	.cmd_rcgr = 0x20004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_0,
	.freq_tbl = ftbl_nw_gcc_gp1_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_gp1_clk_src",
		.parent_data = nw_gcc_parent_data_0,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 nw_gcc_gp2_clk_src = {
	.cmd_rcgr = 0x21004,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_0,
	.freq_tbl = ftbl_nw_gcc_gp1_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_gp2_clk_src",
		.parent_data = nw_gcc_parent_data_0,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_0),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static const struct freq_tbl ftbl_nw_gcc_qupv3_wrap0_s0_clk_src[] = {
	F(7372800, P_NW_GCC_GPLL0_OUT_MAIN, 1, 192, 15625),
	F(14745600, P_NW_GCC_GPLL0_OUT_MAIN, 1, 384, 15625),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(29491200, P_NW_GCC_GPLL0_OUT_MAIN, 1, 768, 15625),
	F(32000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 4, 75),
	F(48000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 2, 25),
	F(51200000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 32, 375),
	F(64000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 8, 75),
	F(66666667, P_NW_GCC_GPLL0_OUT_MAIN, 9, 0, 0),
	F(75000000, P_NW_GCC_GPLL0_OUT_MAIN, 8, 0, 0),
	F(80000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 2, 15),
	F(96000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 4, 25),
	F(100000000, P_NW_GCC_GPLL0_OUT_MAIN, 6, 0, 0),
	F(102400000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 64, 375),
	F(112000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 14, 75),
	F(117964800, P_NW_GCC_GPLL0_OUT_MAIN, 1, 3072, 15625),
	F(120000000, P_NW_GCC_GPLL0_OUT_MAIN, 5, 0, 0),
	{ }
};

static struct clk_init_data nw_gcc_qupv3_wrap0_s0_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap0_s0_clk_src",
	.parent_data = nw_gcc_parent_data_4,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_4),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap0_s0_clk_src = {
	.cmd_rcgr = 0x5616c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_4,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s0_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap0_s0_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap0_s1_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap0_s1_clk_src",
	.parent_data = nw_gcc_parent_data_4,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_4),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap0_s1_clk_src = {
	.cmd_rcgr = 0x562a8,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_4,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s0_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap0_s1_clk_src_init,
};

static const struct freq_tbl ftbl_nw_gcc_qupv3_wrap0_s2_clk_src[] = {
	F(7372800, P_NW_GCC_GPLL0_OUT_MAIN, 1, 192, 15625),
	F(14745600, P_NW_GCC_GPLL0_OUT_MAIN, 1, 384, 15625),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(29491200, P_NW_GCC_GPLL0_OUT_MAIN, 1, 768, 15625),
	F(32000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 4, 75),
	F(48000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 2, 25),
	F(51200000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 32, 375),
	F(64000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 8, 75),
	F(66666667, P_NW_GCC_GPLL0_OUT_MAIN, 9, 0, 0),
	F(75000000, P_NW_GCC_GPLL0_OUT_MAIN, 8, 0, 0),
	F(80000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 2, 15),
	F(96000000, P_NW_GCC_GPLL0_OUT_MAIN, 1, 4, 25),
	F(100000000, P_NW_GCC_GPLL0_OUT_MAIN, 6, 0, 0),
	{ }
};

static struct clk_init_data nw_gcc_qupv3_wrap0_s2_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap0_s2_clk_src",
	.parent_data = nw_gcc_parent_data_4,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_4),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap0_s2_clk_src = {
	.cmd_rcgr = 0x563e4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_4,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap0_s2_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap0_s3_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap0_s3_clk_src",
	.parent_data = nw_gcc_parent_data_4,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_4),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap0_s3_clk_src = {
	.cmd_rcgr = 0x56520,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_4,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap0_s3_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap0_s4_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap0_s4_clk_src",
	.parent_data = nw_gcc_parent_data_4,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_4),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap0_s4_clk_src = {
	.cmd_rcgr = 0x5665c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_4,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap0_s4_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap0_s5_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap0_s5_clk_src",
	.parent_data = nw_gcc_parent_data_4,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_4),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap0_s5_clk_src = {
	.cmd_rcgr = 0x56798,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_4,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap0_s5_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap0_s6_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap0_s6_clk_src",
	.parent_data = nw_gcc_parent_data_4,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_4),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap0_s6_clk_src = {
	.cmd_rcgr = 0x568d4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_4,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap0_s6_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap1_s0_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap1_s0_clk_src",
	.parent_data = nw_gcc_parent_data_5,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_5),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap1_s0_clk_src = {
	.cmd_rcgr = 0x5716c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_5,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s0_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap1_s0_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap1_s1_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap1_s1_clk_src",
	.parent_data = nw_gcc_parent_data_5,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_5),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap1_s1_clk_src = {
	.cmd_rcgr = 0x572a8,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_5,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s0_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap1_s1_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap1_s2_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap1_s2_clk_src",
	.parent_data = nw_gcc_parent_data_5,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_5),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap1_s2_clk_src = {
	.cmd_rcgr = 0x573e4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_5,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap1_s2_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap1_s3_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap1_s3_clk_src",
	.parent_data = nw_gcc_parent_data_5,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_5),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap1_s3_clk_src = {
	.cmd_rcgr = 0x57520,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_5,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap1_s3_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap1_s4_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap1_s4_clk_src",
	.parent_data = nw_gcc_parent_data_5,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_5),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap1_s4_clk_src = {
	.cmd_rcgr = 0x5765c,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_5,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap1_s4_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap1_s5_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap1_s5_clk_src",
	.parent_data = nw_gcc_parent_data_5,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_5),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap1_s5_clk_src = {
	.cmd_rcgr = 0x57798,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_5,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap1_s5_clk_src_init,
};

static struct clk_init_data nw_gcc_qupv3_wrap1_s6_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap1_s6_clk_src",
	.parent_data = nw_gcc_parent_data_5,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_5),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap1_s6_clk_src = {
	.cmd_rcgr = 0x578d4,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_5,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap0_s2_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap1_s6_clk_src_init,
};

static const struct freq_tbl ftbl_nw_gcc_qupv3_wrap3_qspi_ref_clk_src[] = {
	F(7372800, P_NW_GCC_GPLL0_OUT_EVEN, 1, 384, 15625),
	F(14745600, P_NW_GCC_GPLL0_OUT_EVEN, 1, 768, 15625),
	F(19200000, P_BI_TCXO, 1, 0, 0),
	F(29491200, P_NW_GCC_GPLL0_OUT_EVEN, 1, 1536, 15625),
	F(32000000, P_NW_GCC_GPLL0_OUT_EVEN, 1, 8, 75),
	F(48000000, P_NW_GCC_GPLL0_OUT_EVEN, 1, 4, 25),
	F(51200000, P_NW_GCC_GPLL0_OUT_EVEN, 1, 64, 375),
	F(64000000, P_NW_GCC_GPLL0_OUT_EVEN, 1, 16, 75),
	F(75000000, P_NW_GCC_GPLL0_OUT_EVEN, 4, 0, 0),
	F(80000000, P_NW_GCC_GPLL0_OUT_EVEN, 1, 4, 15),
	F(96000000, P_NW_GCC_GPLL0_OUT_EVEN, 1, 8, 25),
	F(100000000, P_NW_GCC_GPLL0_OUT_MAIN, 6, 0, 0),
	F(102400000, P_NW_GCC_GPLL0_OUT_EVEN, 1, 128, 375),
	F(112000000, P_NW_GCC_GPLL0_OUT_EVEN, 1, 28, 75),
	F(117964800, P_NW_GCC_GPLL0_OUT_EVEN, 1, 6144, 15625),
	F(120000000, P_NW_GCC_GPLL0_OUT_MAIN, 5, 0, 0),
	F(150000000, P_NW_GCC_GPLL0_OUT_EVEN, 2, 0, 0),
	F(240000000, P_NW_GCC_GPLL0_OUT_MAIN, 2.5, 0, 0),
	{ }
};

static struct clk_init_data nw_gcc_qupv3_wrap3_qspi_ref_clk_src_init = {
	.name = "nw_gcc_qupv3_wrap3_qspi_ref_clk_src",
	.parent_data = nw_gcc_parent_data_5,
	.num_parents = ARRAY_SIZE(nw_gcc_parent_data_5),
	.flags = CLK_SET_RATE_PARENT,
	.ops = &clk_rcg2_shared_no_init_park_ops,
};

static struct clk_rcg2 nw_gcc_qupv3_wrap3_qspi_ref_clk_src = {
	.cmd_rcgr = 0x58174,
	.mnd_width = 16,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_5,
	.freq_tbl = ftbl_nw_gcc_qupv3_wrap3_qspi_ref_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &nw_gcc_qupv3_wrap3_qspi_ref_clk_src_init,
};

static const struct freq_tbl ftbl_nw_gcc_ufs_0_phy_axi_clk_src[] = {
	F(120000000, P_NW_GCC_GPLL0_OUT_MAIN, 5, 0, 0),
	F(201500000, P_NW_GCC_GPLL3_OUT_MAIN, 4, 0, 0),
	F(300000000, P_NW_GCC_GPLL0_OUT_MAIN, 2, 0, 0),
	F(403000000, P_NW_GCC_GPLL3_OUT_MAIN, 2, 0, 0),
	{ }
};

static struct clk_rcg2 nw_gcc_ufs_0_phy_axi_clk_src = {
	.cmd_rcgr = 0x52034,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_1,
	.freq_tbl = ftbl_nw_gcc_ufs_0_phy_axi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_ufs_0_phy_axi_clk_src",
		.parent_data = nw_gcc_parent_data_1,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 nw_gcc_ufs_0_phy_ice_core_clk_src = {
	.cmd_rcgr = 0x5208c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_1,
	.freq_tbl = ftbl_nw_gcc_ufs_0_phy_axi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_ufs_0_phy_ice_core_clk_src",
		.parent_data = nw_gcc_parent_data_1,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static const struct freq_tbl ftbl_nw_gcc_ufs_0_phy_phy_aux_clk_src[] = {
	F(19200000, P_BI_TCXO, 1, 0, 0),
	{ }
};

static struct clk_rcg2 nw_gcc_ufs_0_phy_phy_aux_clk_src = {
	.cmd_rcgr = 0x520c0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_2,
	.freq_tbl = ftbl_nw_gcc_ufs_0_phy_phy_aux_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_ufs_0_phy_phy_aux_clk_src",
		.parent_data = nw_gcc_parent_data_2,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 nw_gcc_ufs_0_phy_unipro_core_clk_src = {
	.cmd_rcgr = 0x520a4,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_3,
	.freq_tbl = ftbl_nw_gcc_ufs_0_phy_axi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_ufs_0_phy_unipro_core_clk_src",
		.parent_data = nw_gcc_parent_data_1,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 nw_gcc_ufs_1_phy_axi_clk_src = {
	.cmd_rcgr = 0x3d034,
	.mnd_width = 8,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_1,
	.freq_tbl = ftbl_nw_gcc_ufs_0_phy_axi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_ufs_1_phy_axi_clk_src",
		.parent_data = nw_gcc_parent_data_1,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 nw_gcc_ufs_1_phy_ice_core_clk_src = {
	.cmd_rcgr = 0x3d08c,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_1,
	.freq_tbl = ftbl_nw_gcc_ufs_0_phy_axi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_ufs_1_phy_ice_core_clk_src",
		.parent_data = nw_gcc_parent_data_1,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 nw_gcc_ufs_1_phy_phy_aux_clk_src = {
	.cmd_rcgr = 0x3d0c0,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_2,
	.freq_tbl = ftbl_nw_gcc_ufs_0_phy_phy_aux_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_ufs_1_phy_phy_aux_clk_src",
		.parent_data = nw_gcc_parent_data_2,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_2),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_rcg2 nw_gcc_ufs_1_phy_unipro_core_clk_src = {
	.cmd_rcgr = 0x3d0a4,
	.mnd_width = 0,
	.hid_width = 5,
	.parent_map = nw_gcc_parent_map_1,
	.freq_tbl = ftbl_nw_gcc_ufs_0_phy_axi_clk_src,
	.hw_clk_ctrl = true,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_ufs_1_phy_unipro_core_clk_src",
		.parent_data = nw_gcc_parent_data_1,
		.num_parents = ARRAY_SIZE(nw_gcc_parent_data_1),
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_rcg2_shared_ops,
	},
};

static struct clk_regmap_div nw_gcc_qupv3_wrap3_s0_clk_src = {
	.reg = 0x5816c,
	.shift = 0,
	.width = 4,
	.clkr.hw.init = &(const struct clk_init_data) {
		.name = "nw_gcc_qupv3_wrap3_s0_clk_src",
		.parent_hws = (const struct clk_hw*[]) {
			&nw_gcc_qupv3_wrap3_qspi_ref_clk_src.clkr.hw,
		},
		.num_parents = 1,
		.flags = CLK_SET_RATE_PARENT,
		.ops = &clk_regmap_div_ro_ops,
	},
};

static struct clk_branch nw_gcc_aggre_noc_ufs_0_phy_axi_clk = {
	.halt_reg = 0x520f8,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x520f8,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x520f8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_aggre_noc_ufs_0_phy_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_0_phy_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_aggre_noc_ufs_1_phy_axi_clk = {
	.halt_reg = 0x3d0f8,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x3d0f8,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x3d0f8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_aggre_noc_ufs_1_phy_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_1_phy_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_camera_hf_axi_clk = {
	.halt_reg = 0x16008,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x16008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x16008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_camera_hf_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_camera_sf_axi_clk = {
	.halt_reg = 0x1601c,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x1601c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1601c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_camera_sf_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_camera_trig_clk = {
	.halt_reg = 0x16034,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x16034,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x16034,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_camera_trig_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_disp_0_hf_axi_clk = {
	.halt_reg = 0x18008,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x18008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x18008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_disp_0_hf_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_disp_0_trig_clk = {
	.halt_reg = 0x1801c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x1801c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1801c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_disp_0_trig_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_disp_1_hf_axi_clk = {
	.halt_reg = 0x19008,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x19008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x19008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_disp_1_hf_axi_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_disp_1_trig_clk = {
	.halt_reg = 0x1901c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x1901c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1901c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_disp_1_trig_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_dprx0_axi_hf_clk = {
	.halt_reg = 0x29004,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x29004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x29004,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_dprx0_axi_hf_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_eva_axi0_clk = {
	.halt_reg = 0x1b008,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x1b008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1b008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_eva_axi0_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_eva_axi0c_clk = {
	.halt_reg = 0x1b01c,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x1b01c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1b01c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_eva_axi0c_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_eva_trig_clk = {
	.halt_reg = 0x1b028,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x1b028,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1b028,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_eva_trig_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_frq_measure_ref_clk = {
	.halt_reg = 0x1f008,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x1f008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_frq_measure_ref_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_gp1_clk = {
	.halt_reg = 0x20000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x20000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_gp1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_gp1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_gp2_clk = {
	.halt_reg = 0x21000,
	.halt_check = BRANCH_HALT,
	.clkr = {
		.enable_reg = 0x21000,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_gp2_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_gp2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_gpu_gpll0_clk_src = {
	.halt_reg = 0x23018,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x23018,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(4),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_gpu_gpll0_clk_src",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_gpll0.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_gpu_gpll0_div_clk_src = {
	.halt_reg = 0x23020,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x23020,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(5),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_gpu_gpll0_div_clk_src",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_gpll0_out_even.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_core_2x_clk = {
	.halt_reg = 0x56020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(15),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_core_2x_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_core_clk = {
	.halt_reg = 0x5600c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(14),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_core_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_m_ahb_clk = {
	.halt_reg = 0x56004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x56004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(12),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_m_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_s0_clk = {
	.halt_reg = 0x5615c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(16),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_s0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap0_s0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_s1_clk = {
	.halt_reg = 0x56298,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(17),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_s1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap0_s1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_s2_clk = {
	.halt_reg = 0x563d4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(18),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_s2_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap0_s2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_s3_clk = {
	.halt_reg = 0x56510,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(19),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_s3_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap0_s3_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_s4_clk = {
	.halt_reg = 0x5664c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(20),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_s4_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap0_s4_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_s5_clk = {
	.halt_reg = 0x56788,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(21),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_s5_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap0_s5_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_s6_clk = {
	.halt_reg = 0x568c4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(22),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_s6_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap0_s6_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap0_s_ahb_clk = {
	.halt_reg = 0x56008,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x56008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(13),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap0_s_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_core_2x_clk = {
	.halt_reg = 0x57020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(26),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_core_2x_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_core_clk = {
	.halt_reg = 0x5700c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(25),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_core_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_m_ahb_clk = {
	.halt_reg = 0x57004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x57004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(23),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_m_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_s0_clk = {
	.halt_reg = 0x5715c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(27),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_s0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap1_s0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_s1_clk = {
	.halt_reg = 0x57298,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(28),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_s1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap1_s1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_s2_clk = {
	.halt_reg = 0x573d4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(29),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_s2_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap1_s2_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_s3_clk = {
	.halt_reg = 0x57510,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(30),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_s3_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap1_s3_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_s4_clk = {
	.halt_reg = 0x5764c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(31),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_s4_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap1_s4_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_s5_clk = {
	.halt_reg = 0x57788,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_s5_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap1_s5_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_s6_clk = {
	.halt_reg = 0x578c4,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76008,
		.enable_mask = BIT(1),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_s6_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap1_s6_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap1_s_ahb_clk = {
	.halt_reg = 0x57008,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x57008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x76000,
		.enable_mask = BIT(24),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap1_s_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap3_core_2x_clk = {
	.halt_reg = 0x58020,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76008,
		.enable_mask = BIT(4),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap3_core_2x_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap3_core_clk = {
	.halt_reg = 0x5800c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76008,
		.enable_mask = BIT(5),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap3_core_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap3_m_clk = {
	.halt_reg = 0x58004,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x58004,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x76008,
		.enable_mask = BIT(7),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap3_m_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap3_qspi_ref_clk = {
	.halt_reg = 0x58170,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76008,
		.enable_mask = BIT(3),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap3_qspi_ref_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap3_qspi_ref_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap3_s0_clk = {
	.halt_reg = 0x5815c,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x76008,
		.enable_mask = BIT(2),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap3_s0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_qupv3_wrap3_s0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_qupv3_wrap3_s_ahb_clk = {
	.halt_reg = 0x58008,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x58008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x76008,
		.enable_mask = BIT(6),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_qupv3_wrap3_s_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_0_phy_ahb_clk = {
	.halt_reg = 0x52028,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x52028,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x52028,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_0_phy_axi_clk = {
	.halt_reg = 0x52018,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x52018,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x52018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_0_phy_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_0_phy_ice_core_clk = {
	.halt_reg = 0x5207c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x5207c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x5207c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_ice_core_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_0_phy_ice_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_0_phy_phy_aux_clk = {
	.halt_reg = 0x520bc,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x520bc,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x520bc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_phy_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_0_phy_phy_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_0_phy_rx_symbol_0_clk = {
	.halt_reg = 0x52030,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x52030,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_rx_symbol_0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_0_phy_rx_symbol_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_0_phy_rx_symbol_1_clk = {
	.halt_reg = 0x520d8,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x520d8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_rx_symbol_1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_0_phy_rx_symbol_1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_0_phy_tx_symbol_0_clk = {
	.halt_reg = 0x5202c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x5202c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_tx_symbol_0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_0_phy_tx_symbol_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_0_phy_unipro_core_clk = {
	.halt_reg = 0x5206c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x5206c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x5206c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_0_phy_unipro_core_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_0_phy_unipro_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_1_phy_ahb_clk = {
	.halt_reg = 0x3d028,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x3d028,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x3d028,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_ahb_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_1_phy_axi_clk = {
	.halt_reg = 0x3d018,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x3d018,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x3d018,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_axi_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_1_phy_axi_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_1_phy_ice_core_clk = {
	.halt_reg = 0x3d07c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x3d07c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x3d07c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_ice_core_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_1_phy_ice_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_1_phy_phy_aux_clk = {
	.halt_reg = 0x3d0bc,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x3d0bc,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x3d0bc,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_phy_aux_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_1_phy_phy_aux_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_1_phy_rx_symbol_0_clk = {
	.halt_reg = 0x3d030,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x3d030,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_rx_symbol_0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_1_phy_rx_symbol_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_1_phy_rx_symbol_1_clk = {
	.halt_reg = 0x3d0d8,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x3d0d8,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_rx_symbol_1_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_1_phy_rx_symbol_1_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_1_phy_tx_symbol_0_clk = {
	.halt_reg = 0x3d02c,
	.halt_check = BRANCH_HALT_DELAY,
	.clkr = {
		.enable_reg = 0x3d02c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_tx_symbol_0_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_1_phy_tx_symbol_0_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_ufs_1_phy_unipro_core_clk = {
	.halt_reg = 0x3d06c,
	.halt_check = BRANCH_HALT_VOTED,
	.hwcg_reg = 0x3d06c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x3d06c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_ufs_1_phy_unipro_core_clk",
			.parent_hws = (const struct clk_hw*[]) {
				&nw_gcc_ufs_1_phy_unipro_core_clk_src.clkr.hw,
			},
			.num_parents = 1,
			.flags = CLK_SET_RATE_PARENT,
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_video_axi0_clk = {
	.halt_reg = 0x1a008,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x1a008,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1a008,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_video_axi0_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_video_axi0c_clk = {
	.halt_reg = 0x1a01c,
	.halt_check = BRANCH_HALT_SKIP,
	.hwcg_reg = 0x1a01c,
	.hwcg_bit = 1,
	.clkr = {
		.enable_reg = 0x1a01c,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_video_axi0c_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_gpu_smmu_vote_clk = {
	.halt_reg = 0x86038,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x86038,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_gpu_smmu_vote_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct clk_branch nw_gcc_mmu_1_tcu_vote_clk = {
	.halt_reg = 0x86040,
	.halt_check = BRANCH_HALT_VOTED,
	.clkr = {
		.enable_reg = 0x86040,
		.enable_mask = BIT(0),
		.hw.init = &(const struct clk_init_data) {
			.name = "nw_gcc_mmu_1_tcu_vote_clk",
			.ops = &clk_branch2_ops,
		},
	},
};

static struct gdsc nw_gcc_ufs_0_phy_gdsc = {
	.gdscr = 0x52004,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "nw_gcc_ufs_0_phy_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct gdsc nw_gcc_ufs_1_mem_phy_gdsc = {
	.gdscr = 0x3c000,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x2,
	.pd = {
		.name = "nw_gcc_ufs_1_mem_phy_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct gdsc nw_gcc_ufs_0_mem_phy_gdsc = {
	.gdscr = 0x3b000,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0x2,
	.pd = {
		.name = "nw_gcc_ufs_0_mem_phy_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct gdsc nw_gcc_ufs_1_phy_gdsc = {
	.gdscr = 0x3d004,
	.en_rest_wait_val = 0x2,
	.en_few_wait_val = 0x2,
	.clk_dis_wait_val = 0xf,
	.pd = {
		.name = "nw_gcc_ufs_1_phy_gdsc",
	},
	.pwrsts = PWRSTS_OFF_ON,
	.flags = POLL_CFG_GDSCR | RETAIN_FF_ENABLE,
};

static struct clk_regmap *nw_gcc_seca_clocks[] = {
	[NW_GCC_AGGRE_NOC_UFS_0_PHY_AXI_CLK] = &nw_gcc_aggre_noc_ufs_0_phy_axi_clk.clkr,
	[NW_GCC_AGGRE_NOC_UFS_1_PHY_AXI_CLK] = &nw_gcc_aggre_noc_ufs_1_phy_axi_clk.clkr,
	[NW_GCC_CAMERA_HF_AXI_CLK] = &nw_gcc_camera_hf_axi_clk.clkr,
	[NW_GCC_CAMERA_SF_AXI_CLK] = &nw_gcc_camera_sf_axi_clk.clkr,
	[NW_GCC_CAMERA_TRIG_CLK] = &nw_gcc_camera_trig_clk.clkr,
	[NW_GCC_DISP_0_HF_AXI_CLK] = &nw_gcc_disp_0_hf_axi_clk.clkr,
	[NW_GCC_DISP_0_TRIG_CLK] = &nw_gcc_disp_0_trig_clk.clkr,
	[NW_GCC_DISP_1_HF_AXI_CLK] = &nw_gcc_disp_1_hf_axi_clk.clkr,
	[NW_GCC_DISP_1_TRIG_CLK] = &nw_gcc_disp_1_trig_clk.clkr,
	[NW_GCC_DPRX0_AXI_HF_CLK] = &nw_gcc_dprx0_axi_hf_clk.clkr,
	[NW_GCC_EVA_AXI0_CLK] = &nw_gcc_eva_axi0_clk.clkr,
	[NW_GCC_EVA_AXI0C_CLK] = &nw_gcc_eva_axi0c_clk.clkr,
	[NW_GCC_EVA_TRIG_CLK] = &nw_gcc_eva_trig_clk.clkr,
	[NW_GCC_FRQ_MEASURE_REF_CLK] = &nw_gcc_frq_measure_ref_clk.clkr,
	[NW_GCC_GP1_CLK] = &nw_gcc_gp1_clk.clkr,
	[NW_GCC_GP1_CLK_SRC] = &nw_gcc_gp1_clk_src.clkr,
	[NW_GCC_GP2_CLK] = &nw_gcc_gp2_clk.clkr,
	[NW_GCC_GP2_CLK_SRC] = &nw_gcc_gp2_clk_src.clkr,
	[NW_GCC_GPLL0] = &nw_gcc_gpll0.clkr,
	[NW_GCC_GPLL0_OUT_EVEN] = &nw_gcc_gpll0_out_even.clkr,
	[NW_GCC_GPLL3] = &nw_gcc_gpll3.clkr,
	[NW_GCC_GPU_GPLL0_CLK_SRC] = &nw_gcc_gpu_gpll0_clk_src.clkr,
	[NW_GCC_GPU_GPLL0_DIV_CLK_SRC] = &nw_gcc_gpu_gpll0_div_clk_src.clkr,
	[NW_GCC_GPU_SMMU_VOTE_CLK] = &nw_gcc_gpu_smmu_vote_clk.clkr,
	[NW_GCC_MMU_1_TCU_VOTE_CLK] = &nw_gcc_mmu_1_tcu_vote_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_CORE_2X_CLK] = &nw_gcc_qupv3_wrap0_core_2x_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_CORE_CLK] = &nw_gcc_qupv3_wrap0_core_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_M_AHB_CLK] = &nw_gcc_qupv3_wrap0_m_ahb_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_S0_CLK] = &nw_gcc_qupv3_wrap0_s0_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_S0_CLK_SRC] = &nw_gcc_qupv3_wrap0_s0_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP0_S1_CLK] = &nw_gcc_qupv3_wrap0_s1_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_S1_CLK_SRC] = &nw_gcc_qupv3_wrap0_s1_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP0_S2_CLK] = &nw_gcc_qupv3_wrap0_s2_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_S2_CLK_SRC] = &nw_gcc_qupv3_wrap0_s2_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP0_S3_CLK] = &nw_gcc_qupv3_wrap0_s3_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_S3_CLK_SRC] = &nw_gcc_qupv3_wrap0_s3_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP0_S4_CLK] = &nw_gcc_qupv3_wrap0_s4_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_S4_CLK_SRC] = &nw_gcc_qupv3_wrap0_s4_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP0_S5_CLK] = &nw_gcc_qupv3_wrap0_s5_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_S5_CLK_SRC] = &nw_gcc_qupv3_wrap0_s5_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP0_S6_CLK] = &nw_gcc_qupv3_wrap0_s6_clk.clkr,
	[NW_GCC_QUPV3_WRAP0_S6_CLK_SRC] = &nw_gcc_qupv3_wrap0_s6_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP0_S_AHB_CLK] = &nw_gcc_qupv3_wrap0_s_ahb_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_CORE_2X_CLK] = &nw_gcc_qupv3_wrap1_core_2x_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_CORE_CLK] = &nw_gcc_qupv3_wrap1_core_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_M_AHB_CLK] = &nw_gcc_qupv3_wrap1_m_ahb_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_S0_CLK] = &nw_gcc_qupv3_wrap1_s0_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_S0_CLK_SRC] = &nw_gcc_qupv3_wrap1_s0_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP1_S1_CLK] = &nw_gcc_qupv3_wrap1_s1_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_S1_CLK_SRC] = &nw_gcc_qupv3_wrap1_s1_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP1_S2_CLK] = &nw_gcc_qupv3_wrap1_s2_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_S2_CLK_SRC] = &nw_gcc_qupv3_wrap1_s2_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP1_S3_CLK] = &nw_gcc_qupv3_wrap1_s3_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_S3_CLK_SRC] = &nw_gcc_qupv3_wrap1_s3_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP1_S4_CLK] = &nw_gcc_qupv3_wrap1_s4_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_S4_CLK_SRC] = &nw_gcc_qupv3_wrap1_s4_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP1_S5_CLK] = &nw_gcc_qupv3_wrap1_s5_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_S5_CLK_SRC] = &nw_gcc_qupv3_wrap1_s5_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP1_S6_CLK] = &nw_gcc_qupv3_wrap1_s6_clk.clkr,
	[NW_GCC_QUPV3_WRAP1_S6_CLK_SRC] = &nw_gcc_qupv3_wrap1_s6_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP1_S_AHB_CLK] = &nw_gcc_qupv3_wrap1_s_ahb_clk.clkr,
	[NW_GCC_QUPV3_WRAP3_CORE_2X_CLK] = &nw_gcc_qupv3_wrap3_core_2x_clk.clkr,
	[NW_GCC_QUPV3_WRAP3_CORE_CLK] = &nw_gcc_qupv3_wrap3_core_clk.clkr,
	[NW_GCC_QUPV3_WRAP3_M_CLK] = &nw_gcc_qupv3_wrap3_m_clk.clkr,
	[NW_GCC_QUPV3_WRAP3_QSPI_REF_CLK] = &nw_gcc_qupv3_wrap3_qspi_ref_clk.clkr,
	[NW_GCC_QUPV3_WRAP3_QSPI_REF_CLK_SRC] = &nw_gcc_qupv3_wrap3_qspi_ref_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP3_S0_CLK] = &nw_gcc_qupv3_wrap3_s0_clk.clkr,
	[NW_GCC_QUPV3_WRAP3_S0_CLK_SRC] = &nw_gcc_qupv3_wrap3_s0_clk_src.clkr,
	[NW_GCC_QUPV3_WRAP3_S_AHB_CLK] = &nw_gcc_qupv3_wrap3_s_ahb_clk.clkr,
	[NW_GCC_UFS_0_PHY_AHB_CLK] = &nw_gcc_ufs_0_phy_ahb_clk.clkr,
	[NW_GCC_UFS_0_PHY_AXI_CLK] = &nw_gcc_ufs_0_phy_axi_clk.clkr,
	[NW_GCC_UFS_0_PHY_AXI_CLK_SRC] = &nw_gcc_ufs_0_phy_axi_clk_src.clkr,
	[NW_GCC_UFS_0_PHY_ICE_CORE_CLK] = &nw_gcc_ufs_0_phy_ice_core_clk.clkr,
	[NW_GCC_UFS_0_PHY_ICE_CORE_CLK_SRC] = &nw_gcc_ufs_0_phy_ice_core_clk_src.clkr,
	[NW_GCC_UFS_0_PHY_PHY_AUX_CLK] = &nw_gcc_ufs_0_phy_phy_aux_clk.clkr,
	[NW_GCC_UFS_0_PHY_PHY_AUX_CLK_SRC] = &nw_gcc_ufs_0_phy_phy_aux_clk_src.clkr,
	[NW_GCC_UFS_0_PHY_RX_SYMBOL_0_CLK] = &nw_gcc_ufs_0_phy_rx_symbol_0_clk.clkr,
	[NW_GCC_UFS_0_PHY_RX_SYMBOL_0_CLK_SRC] = &nw_gcc_ufs_0_phy_rx_symbol_0_clk_src.clkr,
	[NW_GCC_UFS_0_PHY_RX_SYMBOL_1_CLK] = &nw_gcc_ufs_0_phy_rx_symbol_1_clk.clkr,
	[NW_GCC_UFS_0_PHY_RX_SYMBOL_1_CLK_SRC] = &nw_gcc_ufs_0_phy_rx_symbol_1_clk_src.clkr,
	[NW_GCC_UFS_0_PHY_TX_SYMBOL_0_CLK] = &nw_gcc_ufs_0_phy_tx_symbol_0_clk.clkr,
	[NW_GCC_UFS_0_PHY_TX_SYMBOL_0_CLK_SRC] = &nw_gcc_ufs_0_phy_tx_symbol_0_clk_src.clkr,
	[NW_GCC_UFS_0_PHY_UNIPRO_CORE_CLK] = &nw_gcc_ufs_0_phy_unipro_core_clk.clkr,
	[NW_GCC_UFS_0_PHY_UNIPRO_CORE_CLK_SRC] = &nw_gcc_ufs_0_phy_unipro_core_clk_src.clkr,
	[NW_GCC_UFS_1_PHY_AHB_CLK] = &nw_gcc_ufs_1_phy_ahb_clk.clkr,
	[NW_GCC_UFS_1_PHY_AXI_CLK] = &nw_gcc_ufs_1_phy_axi_clk.clkr,
	[NW_GCC_UFS_1_PHY_AXI_CLK_SRC] = &nw_gcc_ufs_1_phy_axi_clk_src.clkr,
	[NW_GCC_UFS_1_PHY_ICE_CORE_CLK] = &nw_gcc_ufs_1_phy_ice_core_clk.clkr,
	[NW_GCC_UFS_1_PHY_ICE_CORE_CLK_SRC] = &nw_gcc_ufs_1_phy_ice_core_clk_src.clkr,
	[NW_GCC_UFS_1_PHY_PHY_AUX_CLK] = &nw_gcc_ufs_1_phy_phy_aux_clk.clkr,
	[NW_GCC_UFS_1_PHY_PHY_AUX_CLK_SRC] = &nw_gcc_ufs_1_phy_phy_aux_clk_src.clkr,
	[NW_GCC_UFS_1_PHY_RX_SYMBOL_0_CLK] = &nw_gcc_ufs_1_phy_rx_symbol_0_clk.clkr,
	[NW_GCC_UFS_1_PHY_RX_SYMBOL_0_CLK_SRC] = &nw_gcc_ufs_1_phy_rx_symbol_0_clk_src.clkr,
	[NW_GCC_UFS_1_PHY_RX_SYMBOL_1_CLK] = &nw_gcc_ufs_1_phy_rx_symbol_1_clk.clkr,
	[NW_GCC_UFS_1_PHY_RX_SYMBOL_1_CLK_SRC] = &nw_gcc_ufs_1_phy_rx_symbol_1_clk_src.clkr,
	[NW_GCC_UFS_1_PHY_TX_SYMBOL_0_CLK] = &nw_gcc_ufs_1_phy_tx_symbol_0_clk.clkr,
	[NW_GCC_UFS_1_PHY_TX_SYMBOL_0_CLK_SRC] = &nw_gcc_ufs_1_phy_tx_symbol_0_clk_src.clkr,
	[NW_GCC_UFS_1_PHY_UNIPRO_CORE_CLK] = &nw_gcc_ufs_1_phy_unipro_core_clk.clkr,
	[NW_GCC_UFS_1_PHY_UNIPRO_CORE_CLK_SRC] = &nw_gcc_ufs_1_phy_unipro_core_clk_src.clkr,
	[NW_GCC_VIDEO_AXI0_CLK] = &nw_gcc_video_axi0_clk.clkr,
	[NW_GCC_VIDEO_AXI0C_CLK] = &nw_gcc_video_axi0c_clk.clkr,
};

static struct gdsc *nw_gcc_seca_gdscs[] = {
	[NW_GCC_UFS_0_MEM_PHY_GDSC] = &nw_gcc_ufs_0_mem_phy_gdsc,
	[NW_GCC_UFS_0_PHY_GDSC] = &nw_gcc_ufs_0_phy_gdsc,
	[NW_GCC_UFS_1_MEM_PHY_GDSC] = &nw_gcc_ufs_1_mem_phy_gdsc,
	[NW_GCC_UFS_1_PHY_GDSC] = &nw_gcc_ufs_1_phy_gdsc,
};

static const struct qcom_reset_map nw_gcc_seca_resets[] = {
	[NW_GCC_CAMERA_BCR] = { 0x16000 },
	[NW_GCC_DISPLAY_0_BCR] = { 0x18000 },
	[NW_GCC_DISPLAY_1_BCR] = { 0x19000 },
	[NW_GCC_DPRX0_BCR] = { 0x29000 },
	[NW_GCC_EVA_BCR] = { 0x1b000 },
	[NW_GCC_GPU_BCR] = { 0x23000 },
	[NW_GCC_QUPV3_WRAPPER_0_BCR] = { 0x56000 },
	[NW_GCC_QUPV3_WRAPPER_1_BCR] = { 0x57000 },
	[NW_GCC_QUPV3_WRAPPER_3_BCR] = { 0x58000 },
	[NW_GCC_UFS_0_PHY_BCR] = { 0x52000 },
	[NW_GCC_UFS_1_PHY_BCR] = { 0x3d000 },
	[NW_GCC_VIDEO_BCR] = { 0x1a000 },
	[NW_GCC_VIDEO_AXI0_CLK_ARES] = { 0x1a008, 2 },
	[NW_GCC_VIDEO_AXI0C_CLK_ARES] = { 0x1a01c, 2 },
};

static const struct clk_rcg_dfs_data nw_gcc_seca_dfs_clocks[] = {
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap0_s0_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap0_s1_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap0_s2_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap0_s3_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap0_s4_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap0_s5_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap0_s6_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap1_s0_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap1_s1_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap1_s2_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap1_s3_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap1_s4_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap1_s5_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap1_s6_clk_src),
	DEFINE_RCG_DFS(nw_gcc_qupv3_wrap3_qspi_ref_clk_src),
};

static const struct regmap_config nw_gcc_seca_regmap_config = {
	.reg_bits = 32,
	.reg_stride = 4,
	.val_bits = 32,
	.max_register = 0xf41f0,
	.fast_io = true,
};

static void clk_seca_regs_configure(struct device *dev, struct regmap *regmap)
{
	qcom_branch_set_force_mem_core(regmap, nw_gcc_ufs_0_phy_ice_core_clk, true);
	qcom_branch_set_force_mem_core(regmap, nw_gcc_ufs_0_phy_axi_clk, true);
	qcom_branch_set_force_mem_core(regmap, nw_gcc_ufs_1_phy_ice_core_clk, true);
	qcom_branch_set_force_mem_core(regmap, nw_gcc_ufs_1_phy_axi_clk, true);
}

static const u32 nw_gcc_seca_critical_cbcrs[] = {
	0x16004, /* NW_GCC_CAMERA_AHB_CLK */
	0x16030, /* NW_GCC_CAMERA_XO_CLK */
	0x18004, /* NW_GCC_DISP_0_AHB_CLK */
	0x19004, /* NW_GCC_DISP_1_AHB_CLK */
	0x29018, /* NW_GCC_DPRX0_CFG_AHB_CLK */
	0x1b004, /* NW_GCC_EVA_AHB_CLK */
	0x1b024, /* NW_GCC_EVA_XO_CLK */
	0x23004, /* NW_GCC_GPU_CFG_AHB_CLK */
	0x1a004, /* NW_GCC_VIDEO_AHB_CLK */
	0x1a030, /* NW_GCC_VIDEO_XO_CLK */
};

static const struct qcom_cc_driver_data nw_gcc_seca_driver_data = {
	.dfs_rcgs = nw_gcc_seca_dfs_clocks,
	.num_dfs_rcgs = ARRAY_SIZE(nw_gcc_seca_dfs_clocks),
	.clk_regs_configure = clk_seca_regs_configure,
	.clk_cbcrs = nw_gcc_seca_critical_cbcrs,
	.num_clk_cbcrs = ARRAY_SIZE(nw_gcc_seca_critical_cbcrs),
};

static const struct qcom_cc_desc nw_gcc_seca_desc = {
	.config = &nw_gcc_seca_regmap_config,
	.clks = nw_gcc_seca_clocks,
	.num_clks = ARRAY_SIZE(nw_gcc_seca_clocks),
	.resets = nw_gcc_seca_resets,
	.num_resets = ARRAY_SIZE(nw_gcc_seca_resets),
	.gdscs = nw_gcc_seca_gdscs,
	.num_gdscs = ARRAY_SIZE(nw_gcc_seca_gdscs),
	.driver_data = &nw_gcc_seca_driver_data,
};

static const struct of_device_id nw_gcc_seca_match_table[] = {
	{ .compatible = "qcom,seca-nwgcc" },
	{ }
};
MODULE_DEVICE_TABLE(of, nw_gcc_seca_match_table);

static int nw_gcc_seca_probe(struct platform_device *pdev)
{
	return qcom_cc_probe(pdev, &nw_gcc_seca_desc);
}

static struct platform_driver nw_gcc_seca_driver = {
	.probe = nw_gcc_seca_probe,
	.driver = {
		.name = "nwgcc-seca",
		.of_match_table = nw_gcc_seca_match_table,
	},
};

module_platform_driver(nw_gcc_seca_driver);

MODULE_DESCRIPTION("QTI NWGCC Seca driver");
MODULE_LICENSE("GPL");
