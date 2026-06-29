// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <drm/drm_edid.h>
#include <drm/display/drm_dp_mst_helper.h>

#include "dp_mst_drm.h"
#include "dp_panel.h"

struct msm_dp_mst {
	struct drm_dp_mst_topology_mgr mst_mgr;
	struct msm_dp *msm_dp;
	struct drm_dp_aux *dp_aux;
	u32 max_streams;
};

int msm_dp_mst_display_set_mgr_state(struct msm_dp *dp_display, bool state)
{
	struct msm_dp_mst *mst = dp_display->msm_dp_mst;
	int rc;

	rc = drm_dp_mst_topology_mgr_set_mst(&mst->mst_mgr, state);
	if (rc < 0) {
		drm_err(dp_display->drm_dev,
			"[MST] failed to set topology mgr state to %d rc:%d\n", state, rc);
	}

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

	dp_display->msm_dp_mst = mst;
	return 0;
}
