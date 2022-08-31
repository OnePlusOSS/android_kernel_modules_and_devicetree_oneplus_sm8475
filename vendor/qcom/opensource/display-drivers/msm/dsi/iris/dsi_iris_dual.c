// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017-2020, Pixelworks, Inc.
 *
 * These files contain modifications made by Pixelworks, Inc., in 2019-2020.
 */

#include <linux/types.h>
#include <dsi_drm.h>
#include <sde_encoder_phys.h>
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_iris.h"
#include "dsi_iris_api.h"
#include "dsi_iris_lightup.h"
#include "dsi_iris_lightup_ocp.h"
#include "dsi_iris_lp.h"
#include "dsi_iris_pq.h"
#include "dsi_iris_log.h"
#include "dsi_iris_dual.h"
#include "dsi_iris_emv.h"
#include "dsi_iris_memc.h"
#include "dsi_iris_i3c.h"
#include "dsi_iris_cmpt.h"

u32 debug_disable_mipi1_autorefresh;


void iris_pwil0_efifo_setting_reset(void)
{
	u32 value;
	u32 *payload = NULL;

	payload = iris_get_ipopt_payload_data(IRIS_IP_PWIL, 0xf0, 4);
	value = payload[0];
	value &= ~(0x6);
	/* update cmdlist */
	iris_set_ipopt_payload_data(IRIS_IP_PWIL, 0xf0, 4, value);
}

void iris_pwil0_efifo_enable(bool enable)
{
	u32 reg_val;
	u32 *payload = NULL;

	payload = iris_get_ipopt_payload_data(IRIS_IP_PWIL, 0xf0, 4);
	reg_val = payload[0];
	if (enable)
		/* set DATA_PATH_CTRL0->EFIFO_EN,EFIFO_POSITION to 0x3 */
		payload[0] = BITS_SET(reg_val, 2, 1, 0x3);
	else
		/* set DATA_PATH_CTRL0->EFIFO_EN,EFIFO_POSITION to 0 */
		payload[0] = BITS_SET(reg_val, 2, 1, 0);
	iris_set_ipopt_payload_data(IRIS_IP_PWIL, 0xf0, 4, payload[0]);

	//iris_init_update_ipopt_t(IRIS_IP_PWIL, 0xf0, 0xf0, 1);
	iris_set_ipopt_payload_data(IRIS_IP_PWIL, 0xfd, 2, payload[0]);
	iris_init_update_ipopt_t(IRIS_IP_PWIL, 0xfd, 0xfd, 1);

	iris_emv_efifo_enable(enable);
	iris_dma_trig(DMA_CH12, 0);
	iris_update_pq_opt(PATH_DSI, true);
}

void iris_osd_comp_ready_pad_select(bool enable, bool commit)
{
	struct iris_update_regval regval;

	regval.ip = IRIS_IP_SYS;
	regval.opt_id = 0x06;
	regval.mask = 0xe2000;

	if (enable)
		regval.value = 0xe2000;
	else
		regval.value = 0xe0000;

	iris_update_bitmask_regval_nonread(&regval, commit);
}


int iris_mipi_1_data_path_power_on(bool power_on)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	if (power_on) {
		/* pad select: gpio show osd_comp_ready status */
		iris_osd_comp_ready_pad_select(true, true);
#if defined(CONFIG_PXLW_FPGA_IRIS)
		/* it need by FPGApro, will remove on asic */
		if (pcfg->proFPGA_detected)
			iris_pmu_frc_set(true);
#endif
		/* for different efifo size */
		iris_emv_efifo_pre_config();

		/* power on buksram */
		iris_pmu_bsram_set(true);

		/* power on mipi1 data path */
		iris_pmu_mipi2_set(true);

		/* wait mipi1 power on */
		udelay(300);

		/* config mipi1 data path via dma */
		//iris_send_ipopt_cmds(IRIS_IP_DMA, 0xe7);

		/* enable pwil0 elfifo*/
		iris_pwil0_efifo_enable(true);

		/* change blending CSR_TO*/
		if (pcfg->pwil_mode == FRC_MODE)
			iris_blending_cusor_timeout_set(true);

		pcfg->iris_mipi1_power_st = true;

		iris_emv_on_mipi1_up();
	} else {
		/* for different efifo size */
		iris_emv_efifo_restore_config();

		/* disable pwil0 elfifo*/
		iris_pwil0_efifo_enable(false);

		/* change blending CSR_TO*/
		if (pcfg->pwil_mode == FRC_MODE)
			iris_blending_cusor_timeout_set(false);

		/* power off buksram if iris is in pt mode */
		if (pcfg->pwil_mode == PT_MODE)
			iris_pmu_bsram_set(false);

		/* power off mipi1 data path */
		iris_pmu_mipi2_set(false);

		/* pad select: gpio show abypass status */
		iris_osd_comp_ready_pad_select(false, true);

		pcfg->iris_mipi1_power_st = false;
	}

	return 0;
}

void iris_mipi1_enable(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	if (pcfg->iris_mipi1_power_on_pending) {
		mutex_lock(&pcfg->panel->panel_lock);
		iris_mipi_1_data_path_power_on(true);
		mutex_unlock(&pcfg->panel->panel_lock);

		pcfg->iris_mipi1_power_on_pending = false;
	}
}


int iris_dual_blending_enable(bool enable)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	if (enable) {
		/* send pwil meta cmd */
		iris_set_pwil_mode(pcfg->pwil_mode, true, DSI_CMD_SET_STATE_HS, true);
		pcfg->dual_enabled = true;
	} else {
		/* send pwil meta cmd */
		iris_set_pwil_mode(pcfg->pwil_mode, false, DSI_CMD_SET_STATE_HS, true);
		pcfg->dual_enabled = false;
	}
	iris_input_frame_cnt_record();

	return 0;
}

void iris_dual_frc_prepare(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	iris_memc_ctrl_frc_prepare();

	if (!pcfg->iris_mipi1_power_st)
		iris_mipi_1_data_path_power_on(true);

	iris_blending_cusor_timeout_set(true);
}

void iris_single_pt_post(void)
{
	iris_mipi_1_data_path_power_on(false);
	iris_memc_ctrl_pt_post();

	iris_blending_cusor_timeout_set(false);
}

void iris_input_frame_cnt_record(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	struct iris_switch_dump *dump = &pcfg->switch_dump;

	/* recode input frame count when send video meta */
	if (!dump->trigger)
		dump->rx_frame_cnt0 = iris_ocp_read(0xf1a00354, DSI_CMD_SET_STATE_HS);
}

void iris_switch_timeout_dump(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();
	struct iris_switch_dump *dump = &pcfg->switch_dump;

	dump->trigger = true;
	dump->sw_pwr_ctrl = iris_ocp_read(0xf0000068, DSI_CMD_SET_STATE_HS);
	dump->rx_frame_cnt1 = iris_ocp_read(0xf1a00354, DSI_CMD_SET_STATE_HS);
	dump->rx_video_meta = iris_ocp_read(0xf1a00080, DSI_CMD_SET_STATE_HS);
	dump->pwil_cur_meta0 = iris_ocp_read(0xf1940228, DSI_CMD_SET_STATE_HS);
	dump->pwil_status = iris_ocp_read(0xf194015c, DSI_CMD_SET_STATE_HS);
	dump->pwil_disp_ctrl0 = iris_ocp_read(0xf1941000, DSI_CMD_SET_STATE_HS);
	dump->pwil_int = iris_ocp_read(0xf195ffe4, DSI_CMD_SET_STATE_HS);
	dump->dport_int = iris_ocp_read(0xf12dffe4, DSI_CMD_SET_STATE_HS);
	if (dump->sw_pwr_ctrl & 0x10) {
		dump->fi_debugbus[0] = iris_ocp_read(0xf219ffd8, DSI_CMD_SET_STATE_HS);
		dump->fi_debugbus[1] = iris_ocp_read(0xf219ffd8, DSI_CMD_SET_STATE_HS);
		dump->fi_debugbus[2] = iris_ocp_read(0xf219ffd8, DSI_CMD_SET_STATE_HS);
	}

	IRIS_LOGI("sw_pwr_ctrl: %08x, rx_video_meta: %08x", dump->sw_pwr_ctrl, dump->rx_video_meta);
	IRIS_LOGI("rx_frame_cnt0: %08x, rx_frame_cnt1: %08x", dump->rx_frame_cnt0, dump->rx_frame_cnt1);
	IRIS_LOGI("pwil_cur_meta0: %08x, pwil_status: %08x", dump->pwil_cur_meta0, dump->pwil_status);
	IRIS_LOGI("pwil_disp_ctrl0: %08x, pwil_int: %08x", dump->pwil_disp_ctrl0, dump->pwil_int);
	IRIS_LOGI("dport_int: %08x, fi_debugbus[0]: %08x", dump->dport_int, dump->fi_debugbus[0]);
	IRIS_LOGI("fi_debugbus[1]: %08x, fi_debugbus[2]: %08x", dump->fi_debugbus[1], dump->fi_debugbus[2]);
}

int iris_dual_ch_ctrl_cmd_proc(u32 value)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	if (unlikely(!pcfg->panel2)) {
		IRIS_LOGE("%s(), No secondary panel configured!", __func__);
		return -EFAULT;
	}

	switch (value) {
	case DUAL_CTRL_POWER_ON_MIPI_1_DATA_PATH:
		if (!pcfg->frc_enabled)
			iris_frc_lp_switch(true, false);
		if (!pcfg->ap_mipi1_power_st) {
			pcfg->iris_mipi1_power_on_pending = true;
			pcfg->iris_mipi1_power_st = true;
		} else {
			iris_mipi_1_data_path_power_on(true);
		}
		break;
	case DUAL_CTRL_ENABLE_BLENDING:
		iris_dual_blending_enable(true);
		iris_emv_on_dual_open();
		break;
	case DUAL_CTRL_DISABLE_BLENDNG:
		iris_emv_on_dual_closing();
		iris_dual_blending_enable(false);
		iris_emv_on_dual_close();
		break;
	case DUAL_CTRL_POWER_OFF_MIPI_1_DATA_PATH:
		iris_mipi_1_data_path_power_on(false);
		iris_emv_on_mipi1_down();
		if (!pcfg->frc_enabled)
			iris_frc_lp_switch(false, false);
		break;
	case DUAL_CTRL_SWITCH_TIMEOUT:
		IRIS_LOGE("%s(), SWITCH TIMEOUT!", __func__);
		iris_emv_on_dual_closing();
		iris_dual_blending_enable(false);
		iris_emv_on_dual_close();
		iris_switch_timeout_dump();
		iris_health_care();
		break;
	case DUAL_CTRL_DUAL_FRC_PREPRE:
		iris_dual_frc_prepare();
		break;
	case DUAL_CTRL_SINGLE_PT_POST:
		iris_single_pt_post();
		break;
	default:
		break;

	}

	return 0;
}

bool iris_pwil_meta_blend_mode_get(void)
{
	u32 rc = 0;

	rc = iris_ocp_read(IRIS_PWIL_ADDR + PWIL_CUR_META0, DSI_CMD_SET_STATE_HS);
	IRIS_LOGI("IRIS_PWIL_CUR_META0= %x", rc);
	if ((rc != 0) && (rc & BIT(10)))
		return true;

	return false;
}

u32 iris_pwil_mode_state_get(void)
{
	u32 rc = 0;
	bool frc_idle = false;
	struct iris_cfg *pcfg = iris_get_cfg();

	rc = iris_ocp_read(IRIS_PWIL_ADDR + PWIL_STATUS, DSI_CMD_SET_STATE_HS);
	IRIS_LOGI("IRIS_PWIL_STATUS= %x", rc);
	frc_idle = (rc >> 28) & 0x01;
	rc = (rc >> 5) & 0x3f;
	if (rc == 8)
		pcfg->pwil_mode = FRC_MODE;
	else if (rc == 2) {
		if (frc_idle)
			pcfg->pwil_mode = PT_MODE;
		else
			rc = 8; /* if frc is not idle, iris is still in FRC */
	} else if (rc == 4) {
		pcfg->pwil_mode = RFB_MODE;
	}

	return rc;
}

void iris_kernel_multistatus_get(u32 op, u32 count, u32 *values)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	if (pcfg == NULL)
		return;

	if (count == 1)
		*values = iris_kernel_status_get_cmd_proc(op);
	else if (count > 1) {
		if ((op == GET_AP_MIPI_CH_1_STATE) && (count >= 5)) {
			struct iris_panel_timing_info mipi1Timing;

			mipi1Timing.flag = pcfg->ap_mipi1_power_st;
			mipi1Timing.width = pcfg->frc_setting.hres_2nd;
			mipi1Timing.height = pcfg->frc_setting.vres_2nd;
			mipi1Timing.dsc = pcfg->frc_setting.dsc_2nd;
			if (pcfg->panel2 && pcfg->panel2->cur_mode)
				mipi1Timing.fps = pcfg->panel2->cur_mode->timing.refresh_rate;
			else
				mipi1Timing.fps = pcfg->frc_setting.refresh_rate_2nd;
			values[0] = mipi1Timing.flag;
			values[1] = mipi1Timing.width;
			values[2] = mipi1Timing.height;
			values[3] = mipi1Timing.fps;
			values[4] = mipi1Timing.dsc;
		} else if (op == GET_MV_RESOLUTION) {
			values[0] = pcfg->frc_setting.init_single_mv_hres;
			values[1] = pcfg->frc_setting.init_single_mv_vres;
			values[2] = pcfg->frc_setting.init_dual_mv_hres;
			values[3] = pcfg->frc_setting.init_dual_mv_vres;
		} else if ((op == GET_AP_DISP_MODE_STATE) && (count >= 5)) {
			struct iris_panel_timing_info mipi0Timing;

			if (iris_is_curmode_cmd_mode() &&
				pcfg->rx_mode == DSI_OP_CMD_MODE)
				mipi0Timing.flag = 1;
			else if (iris_is_curmode_vid_mode() &&
				pcfg->rx_mode == DSI_OP_VIDEO_MODE)
				mipi0Timing.flag = 2;
			else
				mipi0Timing.flag = 0;

			mipi0Timing.width = pcfg->panel->cur_mode->timing.h_active;
			mipi0Timing.height = pcfg->panel->cur_mode->timing.v_active;
			mipi0Timing.fps = pcfg->panel->cur_mode->timing.refresh_rate;
			mipi0Timing.dsc = pcfg->panel->cur_mode->priv_info->dsc_enabled;
			values[0] = mipi0Timing.flag;
			values[1] = mipi0Timing.width;
			values[2] = mipi0Timing.height;
			values[3] = mipi0Timing.fps;
			values[4] = mipi0Timing.dsc;
		}
	}
}

u32 iris_kernel_status_get_cmd_proc(u32 get_op)
{
	u32 rc = 0;
	struct iris_cfg *pcfg = iris_get_cfg();

	switch (get_op) {
	case GET_IRIS_MIPI_CH_1_STATE:
		rc = pcfg->iris_mipi1_power_st;
		IRIS_LOGI("GET_IRIS_MIPI_CH_1_STATE: %d", rc);
		break;
	case GET_AP_MIPI_CH_1_STATE:
		rc = pcfg->frc_setting.dsc_2nd ? 0x0C : 0x0A;
		rc <<= 8;
		rc |= (pcfg->frc_setting.refresh_rate_2nd & 0xFF);
		rc <<= 4;
		rc |= pcfg->ap_mipi1_power_st;
		IRIS_LOGI("GET_AP_MIPI_CH_1_STATE: 0x%x", rc);
		break;
	case GET_IRIS_BLEND_STATE:
		mutex_lock(&pcfg->panel->panel_lock);
		pcfg->iris_pwil_blend_st = iris_pwil_meta_blend_mode_get();
		if (!iris_emv_on_dual_on(pcfg->iris_pwil_blend_st))
			pcfg->iris_pwil_blend_st = false;
		mutex_unlock(&pcfg->panel->panel_lock);
		rc = pcfg->iris_pwil_blend_st;
		IRIS_LOGI("GET_IRIS_BLEND_STATE: %d", rc);
		break;
	case GET_IRIS_PWIL_MODE_STATE:
		mutex_lock(&pcfg->panel->panel_lock);
		pcfg->iris_pwil_mode_state = iris_pwil_mode_state_get();
		mutex_unlock(&pcfg->panel->panel_lock);
		rc = pcfg->iris_pwil_mode_state;
		if (!iris_emv_on_display_mode(pcfg->iris_pwil_mode_state))
			rc |= 0x100;
		IRIS_LOGI("GET_IRIS_PWIL_MODE_STATE: %d", rc);
		break;
	case GET_AP_MIPI1_AUTO_REFRESH_STATE:
		rc = pcfg->iris_osd_autorefresh_enabled;
		IRIS_LOGI("GET_AP_MIPI1_AUTO_REFRESH_STATE: %d", rc);
		break;
	case GET_IRIS_FI_REPEAT_STATE:
		mutex_lock(&pcfg->panel->panel_lock);
		rc = iris_fi_repeat_state_get();
		mutex_unlock(&pcfg->panel->panel_lock);
		IRIS_LOGI("GET_IRIS_FI_REPEAT_STATE: %d", rc);
		break;
	case GET_AP_DISP_MODE_STATE:
		if (iris_is_curmode_cmd_mode() &&
			pcfg->rx_mode == DSI_OP_CMD_MODE)
			rc = 1;
		else if (iris_is_curmode_vid_mode() &&
			pcfg->rx_mode == DSI_OP_VIDEO_MODE)
			rc = 2;
		else
			rc = 0;
		IRIS_LOGI("GET_AP_DISP_MODE_STATE: %d", rc);
		break;
	case GET_FRC_VFR_STATUS:
		rc = pcfg->memc_info.vfr_en;
		/* disable VFR when frc2pt */
		pcfg->memc_info.vfr_en = false;
		IRIS_LOGI("GET_FRC_VFR_STATUS: %d", rc);
		break;
	default:
		break;
	}

	return rc;
}

void iris_dual_status_clear(bool init)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	if (init) {
		pcfg->dual_enabled = false;

		atomic_set(&pcfg->osd_irq_cnt, 0);
	}
}

irqreturn_t iris_osd_irq_handler(int irq, void *data)
{
	struct dsi_display *display = data;
	struct drm_encoder *enc = NULL;

	if (display == NULL) {
		IRIS_LOGE("%s(), invalid display.", __func__);
		return IRQ_NONE;
	}

	IRIS_LOGV("%s(), irq: %d, display: %s", __func__, irq, display->name);
	if (display && display->bridge)
		enc = display->bridge->base.encoder;

	if (enc)
		sde_encoder_disable_autorefresh_handler(enc);
	else
		IRIS_LOGW("%s(), no encoder.", __func__);

	return IRQ_HANDLED;
}

void iris_inc_osd_irq_cnt(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	atomic_inc(&pcfg->osd_irq_cnt);
	IRIS_LOGD("osd_irq: %d", atomic_read(&pcfg->osd_irq_cnt));
}

void iris_register_osd_irq(void *disp)
{
	int rc = 0;
	int osd_gpio = -1;
	struct dsi_display *display = NULL;
	struct platform_device *pdev = NULL;
	struct iris_cfg *pcfg = NULL;

	if (!iris_is_dual_supported())
		return;

	if (!disp) {
		IRIS_LOGE("%s(), invalid display.", __func__);
		return;
	}

	display = (struct dsi_display *)disp;
	if (!iris_virtual_display(display))
		return;

	pcfg = iris_get_cfg();
	osd_gpio = pcfg->iris_osd_gpio;
	IRIS_LOGI("%s(), for display %s, osd status gpio is %d",
			__func__,
			display->name, osd_gpio);
	if (!gpio_is_valid(osd_gpio)) {
		IRIS_LOGE("%s(%d), osd status gpio not specified",
				__func__, __LINE__);
		return;
	}
	gpio_direction_input(osd_gpio);
	pdev = display->pdev;
	IRIS_LOGI("%s, display: %s, irq: %d", __func__, display->name, gpio_to_irq(osd_gpio));
	rc = devm_request_irq(&pdev->dev, gpio_to_irq(osd_gpio), iris_osd_irq_handler,
			IRQF_TRIGGER_RISING, "OSD_GPIO", display);
	if (rc) {
		IRIS_LOGE("%s(), IRIS OSD request irq failed", __func__);
		return;
	}

	disable_irq(gpio_to_irq(osd_gpio));
}


int iris_osd_auto_refresh_enable(u32 val)
{
	int osd_gpio = -1;
	struct iris_cfg *pcfg = iris_get_cfg();

	IRIS_LOGI("%s(%d), value: %d", __func__, __LINE__, val);

	if (pcfg == NULL) {
		IRIS_LOGE("%s(), no secondary display.", __func__);
		return -EINVAL;
	}

	osd_gpio = pcfg->iris_osd_gpio;
	if (!gpio_is_valid(osd_gpio)) {
		IRIS_LOGE("%s(), invalid GPIO %d", __func__, osd_gpio);
		return -EINVAL;
	}

	if (debug_disable_mipi1_autorefresh) {
		if (pcfg->iris_osd_autorefresh_enabled)
			disable_irq(gpio_to_irq(osd_gpio));
		pcfg->iris_osd_autorefresh_enabled = false;
		IRIS_LOGI("%s(), mipi1 autofresh force disable.", __func__);
		return 0;
	}

	if (val == 1) {
		IRIS_LOGD("%s(), enable osd auto refresh", __func__);
		enable_irq(gpio_to_irq(osd_gpio));
		pcfg->iris_osd_autorefresh_enabled = true;
	} else if (val == 2) {
		IRIS_LOGD("%s(), refresh frame from upper", __func__);
		enable_irq(gpio_to_irq(osd_gpio));
		pcfg->iris_osd_autorefresh_enabled = false;
	} else {
		IRIS_LOGD("%s(), disable osd auto refresh", __func__);
		disable_irq(gpio_to_irq(osd_gpio));
		pcfg->iris_osd_autorefresh_enabled = false;
		iris_osd_irq_cnt_init();
	}

	return 0;
}

int iris_osd_overflow_status_get(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	if (atomic_read(&pcfg->osd_irq_cnt) >= 2)
		pcfg->iris_osd_overflow_st = false;
	else
		pcfg->iris_osd_overflow_st = true;

	return pcfg->iris_osd_overflow_st;
}

void iris_osd_irq_cnt_init(void)
{
	struct iris_cfg *pcfg = iris_get_cfg();

	atomic_set(&pcfg->osd_irq_cnt, 0);
	pcfg->iris_osd_overflow_st = false;
}

bool iris_is_display1_autorefresh_enabled(void *phys_enc)
{
	struct sde_encoder_phys *phys_encoder = phys_enc;
	struct sde_connector *c_conn = NULL;
	struct dsi_display *display = NULL;
	struct iris_cfg *pcfg = NULL;

	if (phys_encoder == NULL)
		return false;

	if (phys_encoder->connector == NULL)
		return false;

	c_conn = to_sde_connector(phys_encoder->connector);
	if (c_conn == NULL)
		return false;

	display = c_conn->display;
	if (display == NULL)
		return false;

	if (!iris_virtual_display(display))
		return false;

	pcfg = iris_get_cfg();
	IRIS_LOGV("%s(), auto refresh: %s", __func__,
			pcfg->iris_osd_autorefresh_enabled ? "true" : "false");
	if (!pcfg->iris_osd_autorefresh_enabled)
		return false;

	iris_osd_irq_cnt_init();

	return true;
}
