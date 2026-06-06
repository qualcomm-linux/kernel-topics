// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <drm/drm_edid.h>
#include <drm/drm_managed.h>
#include <drm/drm_bridge.h>
#include <drm/display/drm_dp_mst_helper.h>

#include "dp_mst_drm.h"
#include "dp_panel.h"

#define MAX_DPCD_TRANSACTION_BYTES 16

#define to_dp_mst_bridge(x)     container_of((x), struct msm_dp_mst_bridge, base)
#define to_dp_mst_bridge_state_priv(x) \
		container_of((x), struct msm_dp_mst_bridge_state, base)
#define to_dp_mst_bridge_state(x) \
		to_dp_mst_bridge_state_priv((x)->obj.state)
#define to_dp_mst_connector(x) \
		container_of((x), struct msm_dp_mst_connector, connector)

#define DP_MST_CONN_ID(x) ((x)->connector ? \
		(x)->connector->base.id : 0)

struct msm_dp_mst_bridge {
	struct drm_bridge base;
	struct drm_private_obj obj;
	u32 id;

	bool initialized;

	struct msm_dp *display;
	struct drm_encoder *encoder;

	struct drm_connector *connector;
	struct msm_dp_panel *msm_dp_panel;
};

struct msm_dp_mst_bridge_state {
	struct drm_private_state base;
	struct drm_connector *connector;
	struct msm_dp_panel *msm_dp_panel;
};

struct msm_dp_mst_connector {
	struct drm_connector connector;
	struct drm_dp_mst_port *mst_port;
	struct msm_dp_mst *dp_mst;
	struct msm_dp_panel *dp_panel;
};

struct msm_dp_mst {
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct msm_dp_mst_bridge *mst_bridge[DP_STREAM_MAX];
	struct msm_dp *msm_dp;
	struct drm_dp_aux *dp_aux;
	u32 max_streams;
	/* Protects MST bridge enable/disable handling. */
	struct mutex mst_lock;
};

static struct drm_private_state *msm_dp_mst_duplicate_bridge_state(struct drm_private_obj *obj)
{
	struct msm_dp_mst_bridge_state *mst_bridge_state;

	mst_bridge_state = kmemdup(obj->state, sizeof(*mst_bridge_state), GFP_KERNEL);
	if (!mst_bridge_state)
		return NULL;

	__drm_atomic_helper_private_obj_duplicate_state(obj, &mst_bridge_state->base);

	return &mst_bridge_state->base;
}

static void msm_dp_mst_destroy_bridge_state(struct drm_private_obj *obj,
					    struct drm_private_state *state)
{
	struct msm_dp_mst_bridge_state *mst_bridge_state =
		to_dp_mst_bridge_state_priv(state);

	kfree(mst_bridge_state);
}

static struct drm_private_state *
msm_dp_mst_create_bridge_state(struct drm_private_obj *obj)
{
	struct msm_dp_mst_bridge_state *mst_bridge_state;

	mst_bridge_state = kzalloc_obj(*mst_bridge_state);
	if (!mst_bridge_state)
		return ERR_PTR(-ENOMEM);

	__drm_atomic_helper_private_obj_create_state(obj, &mst_bridge_state->base);

	return &mst_bridge_state->base;
}

static const struct drm_private_state_funcs msm_dp_mst_bridge_state_funcs = {
	.atomic_create_state    = msm_dp_mst_create_bridge_state,
	.atomic_duplicate_state = msm_dp_mst_duplicate_bridge_state,
	.atomic_destroy_state   = msm_dp_mst_destroy_bridge_state,
};

static struct msm_dp_mst_bridge_state *msm_dp_mst_br_priv_state(struct drm_atomic_state *st,
								struct msm_dp_mst_bridge *bridge)
{
	struct drm_device *dev = bridge->base.dev;
	struct drm_private_state *obj_state = drm_atomic_get_private_obj_state(st, &bridge->obj);

	WARN_ON(!drm_modeset_is_locked(&dev->mode_config.connection_mutex));

	return to_dp_mst_bridge_state_priv(obj_state);
}

static void msm_dp_mst_update_timeslots(struct msm_dp_mst *mst,
					struct msm_dp_mst_bridge *mst_bridge,
					struct drm_atomic_state *state,
					struct drm_dp_mst_port *port)
{
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;

	mst_state = to_drm_dp_mst_topology_state(mst->mst_mgr.base.state);
	payload = drm_atomic_get_mst_payload_state(mst_state, port);

	if (!payload) {
		DRM_ERROR("MST bridge [%d] update_timeslots failed, null payload\n",
			  mst_bridge->id);
		return;
	}

	if (payload->vc_start_slot < 0)
		msm_dp_display_set_stream_info(mst->msm_dp, mst_bridge->msm_dp_panel,
					       mst_bridge->id, 1, 0, 0);
	else
		msm_dp_display_set_stream_info(mst->msm_dp, mst_bridge->msm_dp_panel,
					       mst_bridge->id, payload->vc_start_slot,
					       payload->time_slots, payload->pbn);
}

static int msm_dp_mst_bridge_pre_enable_part1(struct msm_dp_mst_bridge *dp_bridge,
					      struct drm_atomic_state *state)
{
	struct msm_dp *dp_display = dp_bridge->display;
	struct msm_dp_mst *mst = dp_display->msm_dp_mst;
	struct msm_dp_mst_connector *mst_conn = to_dp_mst_connector(dp_bridge->connector);
	struct drm_dp_mst_port *port = mst_conn->mst_port;
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;
	struct msm_dp_panel *dp_panel = mst_conn->dp_panel;
	int pbn;
	int rc = 0;

	mst_state = drm_atomic_get_new_mst_topology_state(state, &mst->mst_mgr);

	pbn = drm_dp_calc_pbn_mode(dp_panel->msm_dp_mode.drm_mode.clock,
				   (mst_conn->connector.display_info.bpc * 3) << 4);

	payload = drm_atomic_get_mst_payload_state(mst_state, port);
	if (!payload || payload->time_slots <= 0) {
		DRM_ERROR("time slots not allocated for conn:%d\n", DP_MST_CONN_ID(dp_bridge));
		rc = -EINVAL;
		return rc;
	}

	drm_dbg_dp(dp_display->drm_dev, "conn:%d pbn:%d, slots:%d\n", DP_MST_CONN_ID(dp_bridge),
		   pbn, payload->time_slots);

	drm_dp_mst_update_slots(mst_state, DP_CAP_ANSI_8B10B);

	rc = drm_dp_add_payload_part1(&mst->mst_mgr, mst_state, payload);
	if (rc) {
		DRM_ERROR("payload allocation failure for conn:%d\n", DP_MST_CONN_ID(dp_bridge));
		return rc;
	}

	msm_dp_mst_update_timeslots(mst, dp_bridge, state, port);

	return rc;
}

static void _msm_dp_mst_bridge_pre_enable_part2(struct msm_dp_mst_bridge *dp_bridge,
						struct drm_atomic_state *state)
{
	struct msm_dp *dp_display = dp_bridge->display;
	struct msm_dp_mst *mst = dp_display->msm_dp_mst;
	struct msm_dp_mst_connector *mst_conn = to_dp_mst_connector(dp_bridge->connector);
	struct drm_dp_mst_port *port = mst_conn->mst_port;
	struct drm_dp_mst_topology_state *mst_state;
	struct drm_dp_mst_atomic_payload *payload;

	drm_dp_check_act_status(&mst->mst_mgr);

	mst_state = to_drm_dp_mst_topology_state(mst->mst_mgr.base.state);
	payload = drm_atomic_get_mst_payload_state(mst_state, port);

	drm_dp_add_payload_part2(&mst->mst_mgr, payload);

	drm_dbg_dp(dp_display->drm_dev, "MST bridge [%d] _pre enable part-2 complete\n",
		   dp_bridge->id);
}

static void msm_dp_mst_bridge_pre_disable_part1(struct msm_dp_mst_bridge *dp_bridge,
						struct drm_atomic_state *state)
{
	struct msm_dp *dp_display = dp_bridge->display;
	struct msm_dp_mst *mst = dp_display->msm_dp_mst;
	struct msm_dp_mst_connector *mst_conn = to_dp_mst_connector(dp_bridge->connector);
	struct drm_dp_mst_port *port = mst_conn->mst_port;
	struct drm_dp_mst_topology_state *old_mst_state;
	struct drm_dp_mst_topology_state *new_mst_state;
	const struct drm_dp_mst_atomic_payload *old_payload;
	struct drm_dp_mst_atomic_payload *new_payload;

	old_mst_state = drm_atomic_get_old_mst_topology_state(state, &mst->mst_mgr);
	new_mst_state = drm_atomic_get_new_mst_topology_state(state, &mst->mst_mgr);

	old_payload = drm_atomic_get_mst_payload_state(old_mst_state, port);
	new_payload = drm_atomic_get_mst_payload_state(new_mst_state, port);

	if (!old_payload || !new_payload) {
		DRM_ERROR("MST bridge [%d] _pre disable part-1 failed, null payload\n",
			  dp_bridge->id);
		return;
	}

	drm_dp_remove_payload_part1(&mst->mst_mgr, new_mst_state, new_payload);
	drm_dp_remove_payload_part2(&mst->mst_mgr, new_mst_state, old_payload, new_payload);

	msm_dp_mst_update_timeslots(mst, dp_bridge, state, port);

	drm_dbg_dp(dp_display->drm_dev, "MST bridge [%d] _pre disable part-1 complete\n",
		   dp_bridge->id);
}

static void msm_dp_mst_bridge_atomic_pre_enable(struct drm_bridge *drm_bridge,
						struct drm_atomic_state *state)
{
	int rc = 0;
	struct msm_dp_mst_bridge *bridge;
	struct msm_dp *dp_display;
	struct msm_dp_mst_bridge_state *mst_bridge_state;
	struct msm_dp_mst *dp_mst;
	struct msm_dp_panel   *msm_dp_panel;

	if (!drm_bridge) {
		DRM_ERROR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	mst_bridge_state = to_dp_mst_bridge_state(bridge);
	dp_display = bridge->display;
	dp_mst = dp_display->msm_dp_mst;

	/* to cover cases of bridge_disable/bridge_enable without modeset */
	bridge->connector = mst_bridge_state->connector;
	bridge->msm_dp_panel = mst_bridge_state->msm_dp_panel;

	if (!bridge->connector) {
		DRM_ERROR("Invalid connector\n");
		return;
	}

	msm_dp_panel = bridge->msm_dp_panel;
	mutex_lock(&dp_mst->mst_lock);

	rc = msm_dp_display_set_mode_helper(dp_display, state, drm_bridge->encoder, msm_dp_panel);
	if (rc) {
		DRM_ERROR("Failed to perform a mode set, rc=%d\n", rc);
		mutex_unlock(&dp_mst->mst_lock);
		return;
	}
	msm_dp_panel->pbn = drm_dp_calc_pbn_mode(msm_dp_panel->msm_dp_mode.drm_mode.clock,
						 msm_dp_panel->msm_dp_mode.bpp << 4);
	rc = msm_dp_display_prepare(dp_display);
	if (rc) {
		DRM_ERROR("[%d] DP display pre-enable failed, rc=%d\n", bridge->id, rc);
		msm_dp_display_unprepare(dp_display);
		mutex_unlock(&dp_mst->mst_lock);
		return;
	}

	rc = msm_dp_mst_bridge_pre_enable_part1(bridge, state);
	if (rc) {
		DRM_ERROR("[%d] DP display pre-enable failed, rc=%d\n", bridge->id, rc);
		mutex_unlock(&dp_mst->mst_lock);
		return;
	}

	msm_dp_display_enable_helper(dp_display, bridge->msm_dp_panel);

	_msm_dp_mst_bridge_pre_enable_part2(bridge, state);

	mutex_unlock(&dp_mst->mst_lock);

	drm_dbg_dp(dp_display->drm_dev, "conn:%d mode:%s pre enable done\n",
		   DP_MST_CONN_ID(bridge), bridge->msm_dp_panel->msm_dp_mode.drm_mode.name);
}

static void msm_dp_mst_bridge_atomic_disable(struct drm_bridge *drm_bridge,
					     struct drm_atomic_state *state)
{
	struct msm_dp_mst_bridge *bridge;
	struct msm_dp *dp_display;
	struct msm_dp_mst *mst;

	if (!drm_bridge) {
		DRM_ERROR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		DRM_ERROR("Invalid connector\n");
		return;
	}

	dp_display = bridge->display;
	mst = dp_display->msm_dp_mst;

	mutex_lock(&mst->mst_lock);

	msm_dp_mst_bridge_pre_disable_part1(bridge, state);

	msm_dp_display_disable_helper(dp_display, bridge->msm_dp_panel);

	drm_dp_check_act_status(&mst->mst_mgr);

	mutex_unlock(&mst->mst_lock);

	drm_dbg_dp(dp_display->drm_dev, "MST bridge:%d disable complete\n", bridge->id);
}

static void msm_dp_mst_bridge_atomic_post_disable(struct drm_bridge *drm_bridge,
						  struct drm_atomic_state *state)
{
	struct msm_dp_mst_bridge *bridge;
	struct msm_dp *dp_display;
	struct msm_dp_mst *mst;

	if (!drm_bridge) {
		DRM_ERROR("Invalid params\n");
		return;
	}

	bridge = to_dp_mst_bridge(drm_bridge);
	if (!bridge->connector) {
		DRM_ERROR("Invalid connector\n");
		return;
	}

	dp_display = bridge->display;
	mst = dp_display->msm_dp_mst;

	mutex_lock(&mst->mst_lock);

	msm_dp_display_atomic_post_disable_helper(dp_display, bridge->msm_dp_panel);

	if (!dp_display->mst_active)
		msm_dp_display_unprepare(dp_display);

	bridge->connector = NULL;
	bridge->msm_dp_panel =  NULL;

	mutex_unlock(&mst->mst_lock);

	drm_dbg_dp(dp_display->drm_dev, "MST bridge:%d conn:%d post disable complete\n",
		   bridge->id, DP_MST_CONN_ID(bridge));
}

static int msm_dp_mst_bridge_atomic_check(struct drm_bridge *drm_bridge,
					  struct drm_bridge_state *bridge_state,
					  struct drm_crtc_state *crtc_state,
					  struct drm_connector_state *conn_state)
{
	struct drm_atomic_state *state = crtc_state->state;
	struct drm_connector *connector = conn_state->connector;
	struct drm_dp_mst_topology_state *mst_state;
	struct msm_dp_mst_connector *mst_conn;
	struct msm_dp_mst *mst;
	int rc = 0, pbn, slots;
	struct msm_dp_mst_bridge_state *mst_bridge_state;
	u32 bpp;

	if (!drm_atomic_crtc_needs_modeset(crtc_state) || !crtc_state->enable)
		return 0;

	mst_conn = to_dp_mst_connector(connector);
	mst = mst_conn->dp_mst;

	bpp = connector->display_info.bpc * 3;

	if (!bpp)
		bpp = 24;

	pbn = drm_dp_calc_pbn_mode(crtc_state->mode.clock, bpp << 4);

	mst_state = to_drm_dp_mst_topology_state(mst->mst_mgr.base.state);
	if (IS_ERR(mst_state))
		return PTR_ERR(mst_state);

	if (!dfixed_trunc(mst_state->pbn_div)) {
		mst_state->pbn_div =
			drm_dp_get_vc_payload_bw(mst_conn->dp_panel->link_info.rate,
						 mst_conn->dp_panel->link_info.num_lanes);
	}

	slots = drm_dp_atomic_find_time_slots(state, &mst->mst_mgr, mst_conn->mst_port, pbn);

	drm_dbg_dp(drm_bridge->dev, "add slots, conn:%d pbn:%d slots:%d rc:%d\n",
		   connector->base.id, pbn, slots, rc);

	if (!conn_state->crtc)  {
		mst_bridge_state = msm_dp_mst_br_priv_state(state, to_dp_mst_bridge(drm_bridge));
		mst_bridge_state->connector = NULL;
		mst_bridge_state->msm_dp_panel = NULL;
	}

	return 0;
}

/* DP MST Bridge APIs */
static const struct drm_bridge_funcs msm_dp_mst_bridge_ops = {
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,
	.atomic_destroy_state   = drm_atomic_helper_bridge_destroy_state,
	.atomic_reset           = drm_atomic_helper_bridge_reset,
	.atomic_pre_enable   = msm_dp_mst_bridge_atomic_pre_enable,
	.atomic_disable      = msm_dp_mst_bridge_atomic_disable,
	.atomic_post_disable = msm_dp_mst_bridge_atomic_post_disable,
	.atomic_check        = msm_dp_mst_bridge_atomic_check,
};

int msm_dp_mst_attach_encoder(struct msm_dp *dp_display, struct drm_encoder *encoder)
{
	int rc = 0;
	struct msm_dp_mst_bridge *bridge = NULL;
	struct drm_device *dev;
	struct msm_dp_mst *mst = dp_display->msm_dp_mst;
	int i;

	for (i = 0; i < mst->max_streams; i++) {
		if (!mst->mst_bridge[i]->initialized) {
			bridge = mst->mst_bridge[i];
			bridge->encoder = encoder;
			bridge->initialized = true;
			bridge->id = i;
			break;
		}
	}

	if (i == mst->max_streams) {
		DRM_ERROR("MST supports only %d bridges\n", mst->max_streams);
		rc = -EACCES;
		goto end;
	}

	dev = dp_display->drm_dev;
	bridge->display = dp_display;
	bridge->base.encoder = encoder;
	bridge->base.type = dp_display->connector_type;
	bridge->base.ops = DRM_BRIDGE_OP_MODES;
	drm_bridge_add(&bridge->base);

	rc = drm_bridge_attach(encoder, &bridge->base, NULL, 0);
	if (rc) {
		DRM_ERROR("failed to attach bridge, rc=%d\n", rc);
		goto end;
	}

	drm_atomic_private_obj_init(dev, &bridge->obj,
				    &msm_dp_mst_bridge_state_funcs);

	drm_dbg_dp(dp_display->drm_dev, "MST drm bridge init. bridge id:%d\n", i);

	return 0;

end:
	return rc;
}

int msm_dp_mst_display_set_mgr_state(struct msm_dp *dp_display, bool state)
{
	struct msm_dp_mst *mst = dp_display->msm_dp_mst;
	int rc;

	rc = drm_dp_mst_topology_mgr_set_mst(&mst->mst_mgr, state);
	if (rc < 0) {
		DRM_ERROR("failed to set topology mgr state to %d. rc %d\n",
			  state, rc);
	}

	drm_dbg_dp(dp_display->drm_dev, "dp_mst_display_set_mgr_state state:%d\n", state);
	return rc;
}

int msm_dp_mst_init(struct msm_dp *dp_display, u32 max_streams, struct drm_dp_aux *drm_aux)
{
	struct drm_device *dev = dp_display->drm_dev;
	int conn_base_id = 0;
	int ret;
	struct msm_dp_mst *msm_dp_mst;

	msm_dp_mst = devm_kzalloc(dev->dev, sizeof(*msm_dp_mst), GFP_KERNEL);
	if (!msm_dp_mst)
		return -ENOMEM;

	memset(&msm_dp_mst->mst_mgr, 0, sizeof(msm_dp_mst->mst_mgr));
	conn_base_id = dp_display->connector->base.id;
	msm_dp_mst->msm_dp = dp_display;
	msm_dp_mst->max_streams = max_streams;

	for (int i = 0; i < DP_STREAM_MAX; i++) {
		msm_dp_mst->mst_bridge[i] =
			devm_drm_bridge_alloc(dev->dev, struct msm_dp_mst_bridge, base,
					      &msm_dp_mst_bridge_ops);
	}

	msm_dp_mst->dp_aux = drm_aux;

	ret = drm_dp_mst_topology_mgr_init(&msm_dp_mst->mst_mgr, dev,
					   drm_aux,
					   MAX_DPCD_TRANSACTION_BYTES,
					   max_streams,
					   conn_base_id);
	if (ret) {
		DRM_ERROR("DP DRM MST topology manager init failed\n");
		return ret;
	}

	dp_display->msm_dp_mst = msm_dp_mst;

	mutex_init(&msm_dp_mst->mst_lock);
	return ret;
}
