// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017-2020, Pixelworks, Inc.
 *
 * These files contain modifications made by Pixelworks, Inc., in 2019-2020.
 */
#include <linux/kobject.h>
#include "dsi_iris_api.h"
#include "dsi_iris_i3c.h"
#include "dsi_iris_loop_back.h"
#include "dsi_iris_lightup.h"
#include "dsi_iris_lightup_ocp.h"
#include "dsi_iris_log.h"
#include "dsi_iris_lp.h"
#include "sde_connector.h"

static void iris_set_esd_status(bool enable)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	struct dsi_display *display = pcfg->display;

	if (!display) {
		return;
	}

	if (!pcfg->panel || !display->drm_conn) {
		return;
	}

	if (!enable) {
		if (display->panel->esd_config.esd_enabled) {
			sde_connector_schedule_status_work(display->drm_conn, false);
			display->panel->esd_config.esd_enabled = false;
			IRIS_LOGD("disable esd work");
		}
	} else {
		if (!display->panel->esd_config.esd_enabled) {
			sde_connector_schedule_status_work(display->drm_conn, true);
			display->panel->esd_config.esd_enabled = true;
			IRIS_LOGD("enabled esd work");
		}
	}
}

static uint32_t iris_loop_back_flag = 0xffffffff;

/**
 * pure loop back
 */
#define PURE_LOOP_BACK_REG_NUM  (51)
static uint32_t pure_loop_back_op_addr[PURE_LOOP_BACK_REG_NUM] = {
0xf00000f8, 0xf0000068, 0xf0000068, 0xf0000068, 0xf0000068,
0xf1480068, 0xf1480074, 0xf1480078, 0xf148007c, 0xf1480080,
0xf14800b8, 0xf14800b4, 0xf1480120, 0xf1a8001c, 0xf1a80020,
0xf1a80024, 0xf1a80028, 0xf1a80060, 0xf1a8005c, 0xf1a9ff00,
0xf1ac0000, 0xf1ac0010, 0xf1ac001c, 0xf1ac0020, 0xf1ac0024,
0xf1ac0028, 0xf1ac0060, 0xf1ac005c, 0xf1adff00, 0xf1980000,
0xf1980008, 0xf1980048, 0xf198007c, 0xf1980050, 0xf1980054,
0xf1981010, 0xf1940000, 0xf1940018, 0xf194006c, 0xf1940008,
0xf19401c4, 0xf194106c, 0xf18c0004, 0xf18d1000, 0xf178001c,
0xf1780128, 0xf1780124, 0xf179ff00, 0xf1700130, 0xf1711000,
0xf1941068
};

static uint32_t pure_loop_back_op_val[PURE_LOOP_BACK_REG_NUM] = {
0x00000644, 0x00000000, 0x00000001, 0x00000003, 0x000000ff,
0x80078030, 0x0e025901, 0xd1002000, 0x0c000700, 0x5c066706,
0x346cf43b, 0xb42b342b, 0x00000002, 0x0e025901, 0xd1002000,
0x0c000700, 0x5c066706, 0x346cf43b, 0xb42b342b, 0x00000100,
0x01480150, 0x80078030, 0x0e025901, 0xd1002000, 0x0c000700,
0x5c066706, 0x346cf43b, 0xb42b342b, 0x00000100, 0x00000a04,
0x00001800, 0x0438021c, 0x00000003, 0x00040000, 0x00040000,
0x00000010, 0x0001400c, 0x0438021c, 0x00000aaa, 0x84400050,
0x00000003, 0x00000200, 0x00000008, 0x00000100, 0x30080800,
0x01040000, 0x10100102, 0x00000100, 0x0a0a0664, 0x00000006,
0x00000037
};

static uint32_t pure_loop_back_op_delay[PURE_LOOP_BACK_REG_NUM] = {
0, 1, 1, 1, 1,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0
};

static uint32_t pure_loop_back_checksum[3] = {0x58f75b40, 0x57ee3d0d, 0x576d2193};

static int iris_pure_loop_back_verify(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	int ret = 0;
	int i = 0;
	uint32_t statis[3] = {0};

	IRIS_LOGD("%s(%d), start.", __func__, __LINE__);
	iris_set_esd_status(false);
	iris_reset();
	mdelay(100);
	mutex_lock(&pcfg->panel->panel_lock);
	iris_set_two_wire0_enable();
	mutex_unlock(&pcfg->panel->panel_lock);

	for (i = 0; i < PURE_LOOP_BACK_REG_NUM; i++) {
		ret = iris_ioctl_i2c_write(pure_loop_back_op_addr[i], pure_loop_back_op_val[i]);
		if (ret) {
			IRIS_LOGE("%s(%d), error, i = %d, ret = %d.", __func__, __LINE__, i, ret);
			return 0x11;
		}

		if (pure_loop_back_op_delay[i])
			mdelay(pure_loop_back_op_delay[i]);
	}

	mdelay(100);
	for (i = 0; i < 3; i++) {
		iris_ioctl_i2c_read(0xf1940200 + i*4, &statis[i]);
		if (ret) {
			IRIS_LOGE("%s(%d), i2c read statis fail.", __func__, __LINE__);
			return 0x12;
		}
	}
	iris_set_esd_status(true);
	IRIS_LOGI("%s(%d), statis = 0x%x 0x%x 0x%x.", __func__, __LINE__,
			statis[0], statis[1], statis[2]);


	if ((statis[0] == pure_loop_back_checksum[0]) &&
		(statis[1] == pure_loop_back_checksum[1]) &&
		(statis[2] == pure_loop_back_checksum[2])) {
		ret = ERR_NO_ERR;
		IRIS_LOGI("%s(%d), loopback validate success.", __func__, __LINE__);
	} else {
		IRIS_LOGE("%s(%d), statis not equal to checksum.", __func__, __LINE__);
		return 0x13;
	}

	IRIS_LOGD("%s(%d), end.", __func__, __LINE__);

	return ret;

}

/**
 * dual pt
 */
#define DUAL_PT_REG_NUM  (47)
static uint32_t dual_pt_op_addr[DUAL_PT_REG_NUM] = {
0xf0000068, 0xf0000068, 0xf0000000, 0xf00000f8, 0xf1480068,
0xf1480074, 0xf1480078, 0xf148007c, 0xf1480080, 0xf14800b8,
0xf14800b4, 0xf1a8001c, 0xf1a80020, 0xf1a80024, 0xf1a80028,
0xf1a80060, 0xf1a8005c, 0xf1ac001c, 0xf1ac0020, 0xf1ac0024,
0xf1ac0028, 0xf1ac0000, 0xf1ac0010, 0xf1ac0060, 0xf1ac005c,
0xf1980000, 0xf1980048, 0xf1980050, 0xf1980054, 0xf1980008,
0xf18c0004, 0xf18d1000, 0xf178001c, 0xf1780124, 0xf1780128,
0xf179ff00, 0xf1700130, 0xf1711000, 0xf198007c, 0xf1981010,
0xf1940000, 0xf1940018, 0xf194006c, 0xf1940008, 0xf19401c4,
0xf194106c, 0xf1941068
};

static uint32_t dual_pt_op_val[DUAL_PT_REG_NUM] = {
0x00000001, 0x000000ff, 0x00000000, 0x00000644, 0x80078030,
0x0e025901, 0xd1002000, 0x0c000700, 0x5c066706, 0x346cf43b,
0xb42b342b, 0x0e025901, 0xd1002000, 0x0c000700, 0x5c066706,
0x346cf43b, 0xb42b342b, 0x0e025901, 0xd1002000, 0x0c000700,
0x5c066706, 0x01480150, 0x80078030, 0x346cf43b, 0xb42b342b,
0x00000a04, 0x0438021c, 0x00040000, 0x00040000, 0x00001800,
0x00000008, 0x00000100, 0x30080800, 0x10100102, 0x01040000,
0x00000100, 0x0a0a0664, 0x00000002, 0x00000003, 0x00000010,
0x0001400c, 0x0438021c, 0x00000aaa, 0x84400050, 0x00000003,
0x00000200, 0x00000037
};

static uint32_t dual_pt_op_delay[DUAL_PT_REG_NUM] = {
0, 1, 0, 1, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0, 0, 0, 0,
0, 0,
};

static uint32_t dual_pt_checksum[3] = {0x58f75b40, 0x57ee3d0d, 0x576d2193};

static int iris_dual_pt_verify(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	int ret = 0;
	int i = 0;
	uint32_t statis[3] = {0};

	IRIS_LOGD("%s(%d), start.", __func__, __LINE__);
	iris_set_esd_status(false);
	iris_reset();
	mdelay(100);
	mutex_lock(&pcfg->panel->panel_lock);
	iris_set_two_wire0_enable();
	mutex_unlock(&pcfg->panel->panel_lock);

	for (i = 0; i < DUAL_PT_REG_NUM; i++) {
		ret = iris_ioctl_i2c_write(dual_pt_op_addr[i], dual_pt_op_val[i]);
		if (ret) {
			IRIS_LOGE("%s(%d), error, i = %d, ret = %d.", __func__, __LINE__, i, ret);
			return 0x21;
		}

		if (dual_pt_op_delay[i])
			mdelay(dual_pt_op_delay[i]);
	}

	mdelay(100);
	for (i = 0; i < 3; i++) {
		iris_ioctl_i2c_read(0xf1940200 + i*4, &statis[i]);
		if (ret) {
			IRIS_LOGE("%s(%d), i2c read statis fail.", __func__, __LINE__);
			return 0x22;
		}
	}
	iris_set_esd_status(true);
	IRIS_LOGI("%s(%d), statis = 0x%x 0x%x 0x%x.", __func__, __LINE__,
			statis[0], statis[1], statis[2]);


	if ((statis[0] == dual_pt_checksum[0]) &&
		(statis[1] == dual_pt_checksum[1]) &&
		(statis[2] == dual_pt_checksum[2])) {
		ret = ERR_NO_ERR;
		IRIS_LOGI("%s(%d), loopback validate success.", __func__, __LINE__);
	} else {
		IRIS_LOGE("%s(%d), statis not equal to checksum.", __func__, __LINE__);
		return 0x23;
	}

	IRIS_LOGD("%s(%d), end.", __func__, __LINE__);

	return ret;
}

int iris_loop_back_validate(void)
{
	int ret = 0;

	if (iris_loop_back_flag & BIT_PURE_LOOPBACK) {
		IRIS_LOGI("%s(%d), step 1, pure loop back verify!", __func__, __LINE__);
		ret = iris_pure_loop_back_verify();
		if (ret) {
			IRIS_LOGE("%s(%d) step1: pure loop back verify ret = %d", __func__, __LINE__, ret);
			return ERR_PURE_LOOP_BACK;
		}
	}

	if (iris_loop_back_flag & BIT_DUAL_PT) {
		IRIS_LOGI("%s(%d), step 2, dual pt verify!", __func__, __LINE__);
		ret = iris_dual_pt_verify();
		if (ret) {
			IRIS_LOGE("%s(%d) step2: dual pt verify ret = %d", __func__, __LINE__, ret);
			return ERR_DUAL_PT;
		}
	}
	iris_reset();
	mdelay(10);
	_iris_bulksram_power_domain_proc();
	_iris_disable_temp_sensor();
	iris_sleep_abyp_power_down();
	return 0;
}

static ssize_t _iris_dbg_loop_back_read(struct file *file,
		char __user *buff, size_t count, loff_t *ppos)
{
	int ret = -1;
	int tot = 0;
	char bp[512];

	if (*ppos)
		return 0;

	ret = iris_loop_back_validate();
	IRIS_LOGI("%s(%d) ret: %d", __func__, __LINE__, ret);

	tot = scnprintf(bp, sizeof(bp), "0x%x\n", ret);
	if (copy_to_user(buff, bp, tot))
		return -EFAULT;

	*ppos += tot;

	return tot;
}

static const struct file_operations iris_loop_back_fops = {
	.open = simple_open,
	.read = _iris_dbg_loop_back_read,
};


int iris_mipi_rx0_validate(void)
{
	int ret = 0;
	uint32_t reg_val = 0;

	iris_ocp_write_val(0xf1a00008, 0xffff3a9a);
	mdelay(100);
	reg_val = iris_ocp_read(0xf1a00004, DSI_CMD_SET_STATE_HS);
	if (reg_val & 0x0ffff3cf)
		ret = 1;
	else
		ret = 0;

	return ret;
}

static ssize_t _iris_mipi_rx_validate(struct file *file, char __user *buff,
		size_t count, loff_t *ppos)
{
	int ret = 0;
	int tot = 0;
	char bp[512];
	struct iris_cfg *pcfg = iris_get_cfg();

	if (*ppos)
		return 0;

	mutex_lock(&pcfg->panel->panel_lock);
	ret = iris_mipi_rx0_validate();
	mutex_unlock(&pcfg->panel->panel_lock);

	tot = scnprintf(bp, sizeof(bp), "0x%02x\n", ret);
	if (copy_to_user(buff, bp, tot))
		return -EFAULT;
	*ppos += tot;

	return tot;

}

static const struct file_operations iris_mipi_rx_validate_fops = {
	.open = simple_open,
	.write = NULL,
	.read = _iris_mipi_rx_validate,
};

static ssize_t iris_loop_back_show(struct kobject *obj, struct kobj_attribute *attr, char *buf)
{
	int ret;

	ret = iris_loop_back_validate();
	IRIS_LOGI("%s(%d) ret: %d", __func__, __LINE__, ret);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", ret);
}

#define IRIS_ATTR(_name, _mode, _show, _store) \
struct kobj_attribute iris_attr_##_name = __ATTR(_name, _mode, _show, _store)
static IRIS_ATTR(iris_loop_back, S_IRUGO, iris_loop_back_show, NULL);

static struct attribute *iris_dev_attrs[] = {
    &iris_attr_iris_loop_back.attr,
    NULL
};

static const struct attribute_group iris_attr_group = {
    .attrs = iris_dev_attrs,
};

int iris_dbgfs_loop_back_init(struct dsi_display *display)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	int retval;
	if (pcfg->iris_kobj == NULL) {
		pcfg->iris_kobj = kobject_create_and_add(IRIS_SYSFS_TOP_DIR, kernel_kobj);
	} else {
		if (pcfg->iris_kobj) {
			/* Create the files associated with this kobject */
			retval = sysfs_create_group(pcfg->iris_kobj, &iris_attr_group);
			if (retval) {
				kobject_put(pcfg->iris_kobj);
				IRIS_LOGW("sysfs create group iris_dbg_display_mode_show error");
			}
		} else {
			IRIS_LOGW("sysfs create group iris dir error");
			return -ENODEV;
		}
	}
	if (pcfg->dbg_root == NULL) {
		pcfg->dbg_root = debugfs_create_dir("iris", NULL);
		if (IS_ERR_OR_NULL(pcfg->dbg_root)) {
			IRIS_LOGE("debugfs_create_dir for iris_debug failed, error %ld",
					PTR_ERR(pcfg->dbg_root));
			return -ENODEV;
		}
	}

	debugfs_create_u32("iris_loop_back_flag", 0644, pcfg->dbg_root,
			(u32 *)&iris_loop_back_flag);

	if (debugfs_create_file("iris_loop_back",	0644, pcfg->dbg_root, display,
				&iris_loop_back_fops) == NULL) {
		IRIS_LOGE("%s(%d): debugfs_create_file: index fail",
				__FILE__, __LINE__);
		return -EFAULT;
	}

	if (debugfs_create_file("iris_mipi_rx_validate",	0644, pcfg->dbg_root, display,
				&iris_mipi_rx_validate_fops) == NULL) {
		IRIS_LOGE("%s(%d): debugfs_create_file: index fail",
				__FILE__, __LINE__);
		return -EFAULT;
	}

	return 0;
}
