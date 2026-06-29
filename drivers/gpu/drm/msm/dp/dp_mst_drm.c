// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <drm/drm_edid.h>
#include <drm/drm_fixed.h>
#include <drm/drm_atomic_helper.h>
#include <drm/display/drm_dp_mst_helper.h>

#include "dp_mst_drm.h"
#include "dp_panel.h"
#include "dpu_encoder.h"

#define to_dp_mst_connector(x) \
		container_of((x), struct msm_dp_mst_connector, connector)

struct msm_dp_mst_encoder {
	struct drm_encoder *enc;
	int stream_id;
	struct msm_dp_panel *dp_panel;
};

struct msm_dp_mst_connector {
	struct drm_connector connector;
	struct drm_dp_mst_port *mst_port;
	struct msm_dp_mst *dp_mst;
};


struct msm_dp_mst {
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct msm_dp_mst_encoder mst_encoders[DP_STREAM_MAX];
	struct msm_dp *msm_dp;
	struct drm_dp_aux *dp_aux;
	u32 max_streams;
	struct mutex mst_lock;
	struct msm_dp_link_info link_info;
};

static struct msm_dp_panel *msm_dp_mst_panel_from_encoder(struct msm_dp_mst *mst,
							  struct drm_encoder *enc)
{
	int i;

	for (i = 0; i < mst->max_streams; i++) {
		if (mst->mst_encoders[i].enc == enc)
			return mst->mst_encoders[i].dp_panel;
	}
	return NULL;
}

static void msm_dp_mst_update_timeslots(struct msm_dp_mst *mst,
					struct msm_dp_panel *panel,
					struct drm_dp_mst_atomic_payload *payload)
{
	if (payload->vc_start_slot < 0)
		msm_dp_display_set_stream_info(mst->msm_dp, panel, 1, 0, 0);
	else
		msm_dp_display_set_stream_info(mst->msm_dp, panel,
					       payload->vc_start_slot,
					       payload->time_slots, payload->pbn);

	drm_dbg_kms(mst->msm_dp->drm_dev,
		    "[MST] stream:%u timeslots vc_start:%d slots:%d pbn:%d\n",
		    panel->stream_id, payload->vc_start_slot,
		    payload->time_slots, payload->pbn);
}

static void msm_dp_mst_stream_enable(struct drm_encoder *encoder,
				     struct drm_atomic_commit *state)
{
	struct drm_connector *connector =
		drm_atomic_get_new_connector_for_encoder(state, encoder);
	struct msm_dp_mst_connector *mst_conn = to_dp_mst_connector(connector);
	struct msm_dp_mst *mst = mst_conn->dp_mst;
	struct msm_dp *dp_display = mst->msm_dp;
	struct msm_dp_panel *panel = msm_dp_mst_panel_from_encoder(mst, encoder);
	struct drm_dp_mst_port *port = mst_conn->mst_port;
	struct drm_dp_mst_topology_state *mst_state =
		drm_atomic_get_new_mst_topology_state(state, &mst->mst_mgr);
	struct drm_dp_mst_atomic_payload *payload =
		drm_atomic_get_mst_payload_state(mst_state, port);
	int rc;

	panel->connector = connector;

	guard(mutex)(&mst->mst_lock);

	rc = msm_dp_display_set_mode_helper(dp_display, state, encoder, panel);
	if (rc) {
		drm_err(dp_display->drm_dev,
			"[MST] stream:%u set_mode failed rc=%d\n", panel->stream_id, rc);
		goto out;
	}

	rc = msm_dp_display_prepare_link(dp_display);
	if (rc) {
		drm_err(dp_display->drm_dev,
			"[MST] stream:%u prepare_link failed rc=%d\n", panel->stream_id, rc);
		msm_dp_display_unprepare(dp_display);
		goto out;
	}

	drm_dp_mst_update_slots(mst_state, DP_CAP_ANSI_8B10B);

	rc = drm_dp_add_payload_part1(&mst->mst_mgr, mst_state, payload);
	if (rc) {
		drm_err(dp_display->drm_dev,
			"[MST] payload allocation failure for conn:%d\n", connector->base.id);
		msm_dp_display_unprepare(dp_display);
		goto out;
	}

	msm_dp_mst_update_timeslots(mst, panel, payload);

	msm_dp_display_enable_helper(dp_display, panel);

	drm_dp_check_act_status(&mst->mst_mgr);

	drm_dp_add_payload_part2(&mst->mst_mgr, payload);

out:
	drm_connector_get(connector);
}

static void msm_dp_mst_stream_disable(struct drm_encoder *encoder,
				      struct drm_atomic_commit *state)
{
	struct drm_connector *connector = drm_atomic_get_old_connector_for_encoder(state, encoder);
	struct msm_dp_mst_connector *mst_conn = to_dp_mst_connector(connector);
	struct msm_dp_mst *mst = mst_conn->dp_mst;
	struct msm_dp_panel *panel = msm_dp_mst_panel_from_encoder(mst, encoder);
	struct drm_dp_mst_topology_state *old_mst_state =
		drm_atomic_get_old_mst_topology_state(state, &mst->mst_mgr);
	struct drm_dp_mst_topology_state *new_mst_state =
		drm_atomic_get_new_mst_topology_state(state, &mst->mst_mgr);
	struct drm_dp_mst_atomic_payload *old_payload =
		drm_atomic_get_mst_payload_state(old_mst_state, mst_conn->mst_port);
	struct drm_dp_mst_atomic_payload *new_payload =
		drm_atomic_get_mst_payload_state(new_mst_state, mst_conn->mst_port);

	guard(mutex)(&mst->mst_lock);

	drm_dp_remove_payload_part1(&mst->mst_mgr, new_mst_state, new_payload);

	drm_dp_remove_payload_part2(&mst->mst_mgr, new_mst_state, old_payload, new_payload);

	msm_dp_mst_update_timeslots(mst, panel, new_payload);

	msm_dp_display_disable_helper(mst->msm_dp, panel);

	drm_dp_check_act_status(&mst->mst_mgr);
}

static void msm_dp_mst_stream_post_disable(struct drm_encoder *encoder,
					   struct drm_atomic_commit *state)
{
	struct drm_connector *connector = drm_atomic_get_old_connector_for_encoder(state, encoder);
	struct msm_dp_mst_connector *mst_conn = to_dp_mst_connector(connector);
	struct msm_dp_mst *mst = mst_conn->dp_mst;
	struct msm_dp_panel *panel = msm_dp_mst_panel_from_encoder(mst, encoder);

	guard(mutex)(&mst->mst_lock);

	msm_dp_display_atomic_post_disable_helper(mst->msm_dp, panel);

	if (!mst->msm_dp->mst_active)
		msm_dp_display_unprepare(mst->msm_dp);

	panel->connector = NULL;

	drm_connector_put(connector);
}

static int msm_dp_mst_enc_atomic_check(struct drm_encoder *enc,
				       struct drm_crtc_state *crtc_state,
				       struct drm_connector_state *conn_state)
{
	struct msm_dp_mst_connector *mst_conn = to_dp_mst_connector(conn_state->connector);
	struct msm_dp_mst *mst = mst_conn->dp_mst;
	struct drm_dp_mst_topology_state *mst_state;
	int bpp, pbn, slots;

	if (!conn_state->crtc)
		return 0;

	if (!drm_atomic_crtc_needs_modeset(crtc_state) || !crtc_state->active)
		return 0;

	bpp = (conn_state->connector->display_info.bpc * 3) ?: 24; /* fallback: assume 8bpc */
	pbn = drm_dp_calc_pbn_mode(crtc_state->mode.clock, bpp << 4);

	mst_state = drm_atomic_get_mst_topology_state(crtc_state->state, &mst->mst_mgr);
	if (IS_ERR(mst_state))
		return PTR_ERR(mst_state);

	if (!dfixed_trunc(mst_state->pbn_div)) {
		mst_state->pbn_div =
			drm_dp_get_vc_payload_bw(mst->link_info.rate,
						 mst->link_info.num_lanes);
	}

	slots = drm_dp_atomic_find_time_slots(crtc_state->state, &mst->mst_mgr,
					      mst_conn->mst_port, pbn);
	if (slots < 0)
		return slots;

	return 0;
}

static void msm_dp_mst_enc_atomic_enable(struct drm_encoder *enc,
					 struct drm_atomic_commit *state)
{
	msm_dp_mst_stream_enable(enc, state);
	dpu_encoder_phys_enable(enc, state);
}

static void msm_dp_mst_enc_atomic_disable(struct drm_encoder *enc,
					  struct drm_atomic_commit *state)
{
	msm_dp_mst_stream_disable(enc, state);
	dpu_encoder_phys_disable(enc, state);
	msm_dp_mst_stream_post_disable(enc, state);
}

static const struct drm_encoder_helper_funcs msm_dp_mst_encoder_helper_funcs = {
	.atomic_check    = msm_dp_mst_enc_atomic_check,
	.atomic_mode_set = dpu_encoder_atomic_mode_set,
	.atomic_enable   = msm_dp_mst_enc_atomic_enable,
	.atomic_disable  = msm_dp_mst_enc_atomic_disable,
};

int msm_dp_mst_attach_encoder(struct msm_dp *dp_display, struct drm_encoder *encoder)
{
	struct msm_dp_mst *mst = dp_display->msm_dp_mst;
	struct msm_dp_panel *dp_panel;
	int i;

	for (i = 0; i < mst->max_streams; i++) {
		if (!mst->mst_encoders[i].enc)
			break;
	}

	dp_panel = msm_dp_display_get_panel(dp_display, i);
	if (!dp_panel) {
		drm_err(dp_display->drm_dev,
			"[MST] failed to allocate panel for stream %d\n", i);
		return -ENOMEM;
	}

	mst->mst_encoders[i].enc = encoder;
	mst->mst_encoders[i].stream_id = i;
	mst->mst_encoders[i].dp_panel = dp_panel;
	drm_encoder_helper_add(encoder, &msm_dp_mst_encoder_helper_funcs);

	return 0;
}

int msm_dp_mst_display_set_mgr_state(struct msm_dp *dp_display, bool state)
{
	struct msm_dp_mst *mst = dp_display->msm_dp_mst;
	int rc;

	rc = drm_dp_mst_topology_mgr_set_mst(&mst->mst_mgr, state);
	if (rc < 0) {
		drm_err(dp_display->drm_dev,
			"[MST] failed to set topology mgr state to %d rc:%d\n", state, rc);
	}

	if (state)
		msm_dp_display_set_link_info(dp_display, &mst->link_info);

	drm_dbg_kms(dp_display->drm_dev, "[MST] set_mgr_state state:%d\n", state);
	return rc;
}

int msm_dp_mst_init(struct msm_dp *dp_display, u32 max_streams, struct drm_dp_aux *drm_aux)
{
	struct drm_device *dev = dp_display->drm_dev;
	struct msm_dp_mst *mst;
	int ret;

	mst = devm_kzalloc(dev->dev, sizeof(*mst), GFP_KERNEL);
	if (!mst)
		return -ENOMEM;

	mst->msm_dp = dp_display;
	mst->max_streams = max_streams;
	mst->dp_aux = drm_aux;

	ret = drm_dp_mst_topology_mgr_init(&mst->mst_mgr, dev,
					   drm_aux,
					   16,
					   max_streams,
					   dp_display->connector->base.id);
	if (ret) {
		drm_err(dev, "[MST] topology manager init failed\n");
		return ret;
	}

	mutex_init(&mst->mst_lock);
	dp_display->msm_dp_mst = mst;
	return 0;
}
