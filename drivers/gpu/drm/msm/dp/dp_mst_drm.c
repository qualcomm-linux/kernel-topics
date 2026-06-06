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

struct msm_dp_mst {
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct msm_dp *msm_dp;
	struct drm_dp_aux *dp_aux;
	u32 max_streams;
	/* Protects MST bridge enable/disable handling. */
	struct mutex mst_lock;
};

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
