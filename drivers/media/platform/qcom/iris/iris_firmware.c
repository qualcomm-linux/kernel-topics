// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2022-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/firmware.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/soc/qcom/mdt_loader.h>

#include "iris_core.h"
#include "iris_firmware.h"

#define IRIS_PAS_ID				9

#define MAX_FIRMWARE_NAME_SIZE	128

/* Detect Gen2 firmware by scanning the blob for:
 *   QC_IMAGE_VERSION_STRING=<version>
 * and then checking:
 *   - version starts with "vfw", OR
 *   - version matches "video-firmware.N.M" with N >= 2
 */

static bool iris_detect_gen2_from_fwdata(const u8 *data, size_t size)
{
	static const char *marker = "QC_IMAGE_VERSION_STRING=";
	const size_t mlen = strlen(marker);
	static const char *vfw = "vfw";
	const size_t vfwlen = strlen(vfw);
	static const char *vf = "video-firmware.";
	const size_t vflen = strlen(vf);

	for (size_t i = 0; i + mlen < size; i++) {
		const char *found;

		if (memcmp(data + i, marker, mlen))
			continue;

		found = data + i + mlen;
		size -= i + mlen;

		/* vfw => Gen2 */
		if (size > vfwlen && !memcmp(found, vfw, vfwlen))
			return true;

		if (size < vflen ||
		    memcmp(found, vf, vflen))
			return false;

		found += vflen;
		size -= vflen;

		/*
		 * video-firmware.1.x is Gen1.
		 * video-firmware.2.x and video-firmware.10.x are Gen2.
		 */
		return size >= 2 &&
			(*found >= '2' || (*found == '1' && found[1] != '.'));
	}

	return false;
}

static const struct firmware *iris_detect_firmware(struct iris_core *core,
						   const char **fw_name)
{
	const struct iris_firmware_desc *desc;
	const struct firmware *firmware;
	bool has_both_gens;
	int ret;

	*fw_name = NULL;
	ret = of_property_read_string_index(dev_of_node(core->dev), "firmware-name", 0, fw_name);

	/*
	 * A platform may support both Gen1 and Gen2 firmware; which one is used
	 * depends on the firmware image installed on the system, not on the
	 * hardware. That installed image does not change while the device is
	 * bound, so detect the generation only once and reuse the chosen
	 * descriptor on later core bring-ups (e.g. after a system error
	 * recovery). Besides avoiding the redundant probing, this ensures
	 * core->iris_firmware_desc and iris_firmware_data are published exactly
	 * once, before any session exists, so the lockless readers in the ioctl
	 * paths never observe a reassignment.
	 */
	if (core->iris_firmware_desc) {
		if (ret)
			*fw_name = core->iris_firmware_desc->fwname;
		ret = request_firmware(&firmware, *fw_name, core->dev);
		return ret ? ERR_PTR(ret) : firmware;
	}

	has_both_gens = core->iris_platform_data->firmware_desc_gen2 &&
		core->iris_platform_data->firmware_desc_gen1;

	if (core->iris_platform_data->firmware_desc_gen2)
		desc = core->iris_platform_data->firmware_desc_gen2;
	else if (core->iris_platform_data->firmware_desc_gen1)
		desc = core->iris_platform_data->firmware_desc_gen1;
	else
		return ERR_PTR(-EINVAL);

	if (ret) {
		/* No firmware-name in DT: select by probing Gen2 then Gen1. */
		*fw_name = desc->fwname;
		if (has_both_gens)
			ret = firmware_request_nowarn(&firmware, *fw_name, core->dev);
		else
			ret = request_firmware(&firmware, *fw_name, core->dev);
		if (ret && has_both_gens) {
			desc = core->iris_platform_data->firmware_desc_gen1;
			*fw_name = desc->fwname;
			ret = request_firmware(&firmware, *fw_name, core->dev);
		}
		if (ret)
			return ERR_PTR(ret);
	} else {
		/* firmware-name given: load it and detect its generation. */
		ret = request_firmware(&firmware, *fw_name, core->dev);
		if (ret)
			return ERR_PTR(ret);

		if (has_both_gens &&
		    !iris_detect_gen2_from_fwdata((const u8 *)firmware->data, firmware->size)) {
			dev_info(core->dev, "Gen1 FW detected in %s\n", *fw_name);
			desc = core->iris_platform_data->firmware_desc_gen1;
		}
	}

	/* Publish iris_firmware_data first, then iris_firmware_desc (the guard). */
	core->iris_firmware_data = desc->firmware_data;
	core->iris_firmware_desc = desc;

	return firmware;
}

static int iris_load_fw_to_memory(struct iris_core *core)
{
	const struct firmware *firmware = NULL;
	struct device *dev = core->dev;
	struct resource res;
	phys_addr_t mem_phys;
	const char *fw_name;
	size_t res_size;
	ssize_t fw_size;
	void *mem_virt;
	int ret;

	ret = of_reserved_mem_region_to_resource(dev->of_node, 0, &res);
	if (ret)
		return ret;

	mem_phys = res.start;
	res_size = resource_size(&res);

	firmware = iris_detect_firmware(core, &fw_name);
	if (IS_ERR(firmware))
		return PTR_ERR(firmware);

	fw_size = qcom_mdt_get_size(firmware);
	if (fw_size < 0 || res_size < (size_t)fw_size) {
		ret = -EINVAL;
		goto err_release_fw;
	}

	mem_virt = memremap(mem_phys, res_size, MEMREMAP_WC);
	if (!mem_virt) {
		ret = -ENOMEM;
		goto err_release_fw;
	}

	ret = qcom_mdt_load(dev, firmware, fw_name,
			    IRIS_PAS_ID, mem_virt, mem_phys, res_size, NULL);

	memunmap(mem_virt);
err_release_fw:
	release_firmware(firmware);

	return ret;
}

int iris_fw_load(struct iris_core *core)
{
	const struct tz_cp_config *cp_config;
	int i, ret;

	ret = iris_load_fw_to_memory(core);
	if (ret) {
		dev_err(core->dev, "firmware download failed %d\n", ret);
		return ret;
	}

	ret = qcom_scm_pas_auth_and_reset(IRIS_PAS_ID);
	if (ret)  {
		dev_err(core->dev, "auth and reset failed: %d\n", ret);
		return ret;
	}

	for (i = 0; i < core->iris_platform_data->tz_cp_config_data_size; i++) {
		cp_config = &core->iris_platform_data->tz_cp_config_data[i];
		ret = qcom_scm_mem_protect_video_var(cp_config->cp_start,
						     cp_config->cp_size,
						     cp_config->cp_nonpixel_start,
						     cp_config->cp_nonpixel_size);
		if (ret) {
			dev_err(core->dev, "qcom_scm_mem_protect_video_var failed: %d\n", ret);
			qcom_scm_pas_shutdown(IRIS_PAS_ID);
			return ret;
		}
	}

	return 0;
}

int iris_fw_unload(struct iris_core *core)
{
	return qcom_scm_pas_shutdown(IRIS_PAS_ID);
}

int iris_set_hw_state(struct iris_core *core, bool resume)
{
	return qcom_scm_set_remote_state(resume, 0);
}
