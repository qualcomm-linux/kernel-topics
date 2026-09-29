/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __IRIS_PLATFORM_SM8550_H__
#define __IRIS_PLATFORM_SM8550_H__

static const char * const sm8550_clk_reset_table[] = { "bus" };

static const struct iris_power_domain_data sm8550_ctrl_data = {
	.pd_names = (const char *[]) {
		"venus",
	},
	.pd_cnt = 1,
	.clk_names = (const char *[]) {
		"iface", "core",
	},
	.clk_cnt = 2,
};

static const struct iris_power_domain_data sm8550_vcodec_data[] = {
	{
		.pd_names = (const char *[]) {
			"vcodec0",
		},
		.pd_cnt = 1,
		.clk_names = (const char *[]) {
			"vcodec0_core",
		},
		.clk_cnt = 1,
	},
};

static struct platform_inst_caps platform_inst_cap_sm8550 = {
	.min_frame_width = 96,
	.max_frame_width = 8192,
	.min_frame_height = 96,
	.max_frame_height = 8192,
	.max_mbpf = (8192 * 4352) / 256,
	.mb_cycles_vpp = 200,
	.mb_cycles_fw = 489583,
	.mb_cycles_fw_vpp = 66234,
	.max_frame_rate = MAXIMUM_FPS,
	.max_operating_rate = MAXIMUM_FPS,
};

static const struct iris_vsp_freq_tbl iris_vsp_freq_tbl = {
	.min_freq = (const u32[]) {444, 444, 444, 533},
	.pixel_count = (const u64[]) {
		PIXEL_COUNT(3840, 2160, 240),
		PIXEL_COUNT(3840, 2160, 240),
		PIXEL_COUNT(3840, 2160, 180),
		PIXEL_COUNT(3840, 2160, 120),
		PIXEL_COUNT(3840, 2160, 90),
		PIXEL_COUNT(3840, 2160, 60),
		PIXEL_COUNT(3840, 2160, 30),
		PIXEL_COUNT(1920, 1080, 60),
		PIXEL_COUNT(1920, 1080, 30),
		PIXEL_COUNT(1280, 720, 30),
	},
	.pixel_count_size = 10,
	.ref_bitrate = (const u32 * const[]) {
		(const u32[]) {10, 140, 150, 160, 175, 190, 190, 190, 190, 190},	/* AVC */
		(const u32[]) {90, 140, 160, 180, 190, 200, 200, 200, 200, 200},	/* HEVC */
		(const u32[]) {90, 90, 90, 90, 90, 90, 90, 90, 90, 90},			/* VP9 */
		(const u32[]) {130, 130, 120, 120, 120, 120, 120, 120, 120, 120},	/* AV1 */
	},
};

#endif
