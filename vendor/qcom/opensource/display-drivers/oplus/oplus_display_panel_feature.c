/***************************************************************
** Copyright (C),  2022,  oplus Mobile Comm Corp.,  Ltd
**
** File : oplus_display_panel_feature.c
** Description : oplus display panel char dev  /dev/oplus_panel
** Version : 1.0
******************************************************************/
#include <drm/drm_mipi_dsi.h>
#include "dsi_parser.h"
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_clk.h"
#include "oplus_bl.h"
/* OPLUS_FEATURE_ADFR, include header file*/
#include "oplus_adfr.h"
#include "oplus_display_panel_feature.h"
#include "oplus_onscreenfingerprint.h"
#include "oplus_display_panel_hbm.h"
#include "oplus_display_private_api.h"

#if defined(CONFIG_PXLW_IRIS)
#include "../../msm/dsi/iris/dsi_iris_api.h"
#endif

extern int lcd_closebl_flag;
extern u32 oplus_last_backlight;

int oplus_panel_get_serial_number_info(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = NULL;
	int ret = 0;
	if (!panel){
		DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}
	utils = &panel->utils;

	panel->oplus_ser.serial_number_support = utils->read_bool(utils->data,
			"oplus,dsi-serial-number-enabled");
	DSI_INFO("oplus,dsi-serial-number-enabled: %s", panel->oplus_ser.serial_number_support ? "true" : "false");

	if (panel->oplus_ser.serial_number_support) {
		panel->oplus_ser.is_reg_lock = utils->read_bool(utils->data, "oplus,dsi-serial-number-lock");
		DSI_INFO("oplus,dsi-serial-number-lock: %s", panel->oplus_ser.is_reg_lock ? "true" : "false");

		ret = utils->read_u32(utils->data, "oplus,dsi-serial-number-reg",
				&panel->oplus_ser.serial_number_reg);
		if (ret) {
			pr_info("[%s] failed to get oplus,dsi-serial-number-reg\n", __func__);
			panel->oplus_ser.serial_number_reg = 0xA1;
		}

		ret = utils->read_u32(utils->data, "oplus,dsi-serial-number-index",
				&panel->oplus_ser.serial_number_index);
		if (ret) {
			pr_info("[%s] failed to get oplus,dsi-serial-number-index\n", __func__);
			/* Default sync start index is set 5 */
			panel->oplus_ser.serial_number_index = 7;
		}

		ret = utils->read_u32(utils->data, "oplus,dsi-serial-number-read-count",
				&panel->oplus_ser.serial_number_conut);
		if (ret) {
			pr_info("[%s] failed to get oplus,dsi-serial-number-read-count\n", __func__);
			/* Default  read conut 5 */
			panel->oplus_ser.serial_number_conut = 5;
		}
	}
	return 0;
}

int oplus_panel_features_config(struct dsi_panel *panel)
{
	struct dsi_parser_utils *utils = NULL;
	if (!panel){
		DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}
	utils = &panel->utils;
	panel->oplus_priv.dp_support = utils->get_property(utils->data,
			"oplus,dp-enabled", NULL);

	if (!panel->oplus_priv.dp_support) {
		pr_info("Failed to found panel dp support, using null dp config\n");
		panel->oplus_priv.dp_support = false;
	}

	panel->oplus_priv.cabc_enabled = utils->read_bool(utils->data,
			"oplus,dsi-cabc-enabled");
	DSI_INFO("oplus,dsi-cabc-enabled: %s", panel->oplus_priv.cabc_enabled ? "true" : "false");

	panel->oplus_priv.dre_enabled = utils->read_bool(utils->data,
			"oplus,dsi-dre-enabled");
	DSI_INFO("oplus,dsi-dre-enabled: %s", panel->oplus_priv.dre_enabled ? "true" : "false");

	oplus_panel_get_serial_number_info(panel);

	return 0;
}

int oplus_panel_post_on_backlight(void *display, struct dsi_panel *panel, u32 bl_lvl)
{
	struct dsi_display *dsi_display = display;
	int rc = 0;
	if (!panel || !dsi_display){
		DSI_ERR("oplus post backlight No panel device\n");
		return -ENODEV;
	}

	if ((bl_lvl == 0 && panel->bl_config.bl_level != 0) ||
		(bl_lvl != 0 && panel->bl_config.bl_level == 0)) {
		pr_info("backlight level changed %d -> %d\n",
				panel->bl_config.bl_level, bl_lvl);
	} else if (panel->bl_config.bl_level == 1) {
		pr_info("aod backlight level changed %d -> %d\n",
				panel->bl_config.bl_level, bl_lvl);
	}

	/* Add some delay to avoid screen flash */
	if (panel->need_power_on_backlight && bl_lvl) {
		panel->need_power_on_backlight = false;
		rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_ON);
		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_POST_ON_BACKLIGHT cmds, rc=%d\n",
				panel->name, rc);
		}
		rc = dsi_panel_tx_cmd_set(panel, DSI_CMD_POST_ON_BACKLIGHT);

		rc = dsi_display_clk_ctrl(dsi_display->dsi_clk_handle,
			DSI_CORE_CLK, DSI_CLK_OFF);

		atomic_set(&panel->esd_pending, 0);

		if (rc) {
			pr_err("[%s] failed to send DSI_CMD_POST_ON_BACKLIGHT cmds, rc=%d\n",
				panel->name, rc);
		}

		if ((!strcmp(panel->name, "BOE AB319 NT37701B UDC") || !strcmp(panel->name, "BOE AB241 NT37701A"))
			&& get_oplus_display_power_status() == OPLUS_DISPLAY_POWER_ON)
			oplus_panel_event_notification_trigger(display, DRM_PANEL_EVENT_UNBLANK);
	}
	return 0;
}

u32 oplus_panel_silence_backlight(struct dsi_panel *panel, u32 bl_lvl)
{
	u32 bl_temp = 0;
	if (!panel){
		DSI_ERR("Oplus Features config No panel device\n");
		return -ENODEV;
	}

	bl_temp = bl_lvl;

	if (lcd_closebl_flag) {
		pr_err("silence reboot we should set backlight to zero\n");
		bl_temp = 0;
	} else if (bl_lvl) {
		if (oplus_get_ofp_switch_mode() == OFP_SWITCH_OFF) {
			oplus_set_ofp_switch_mode(OFP_SWITCH_ON);
			DSI_DEBUG("oplus ofp switch on\n");
		}
	}
	return bl_temp;
}

void oplus_panel_update_backlight(struct mipi_dsi_device *dsi, u32 bl_lvl)
{
	int rc = 0;
	struct dsi_cmd_desc *cmds;
	struct mipi_dsi_msg msg;
	char *tx_buf = NULL;
	struct dsi_panel_cmd_set *cmd_sets;
	u64 inverted_dbv_bl_lvl = 0;
	struct dsi_display *dsi_display = get_main_display();

	if (!dsi_display || !dsi_display->panel ||
		!dsi_display->panel->cur_mode || !dsi_display->panel->cur_mode->priv_info) {
		DSI_ERR("Oplus Features config No panel device\n");
		return;
	}

	if ((get_oplus_display_scene() == OPLUS_DISPLAY_AOD_SCENE) && ( bl_lvl == 1)) {
		pr_err("dsi_cmd AOD mode return bl_lvl:%d\n",bl_lvl);
		return;
	}

	oplus_display_backlight_notifier(dsi_display, bl_lvl);

	if (dsi_display->panel->is_hbm_enabled && (bl_lvl != 0))
		return;

	if((bl_lvl == 0) && oplus_display_get_hbm_mode()) {
		pr_info("set backlight 0 and recovery hbm to 0\n");
			__oplus_display_set_hbm(0);
	}

	if (oplus_display_get_hbm_mode()) {
		return;
	}

	oplus_panel_global_hbm_mapping(dsi_display->panel, &bl_lvl);

	if (dsi_display->panel->bl_config.bl_inverted_dbv)
		inverted_dbv_bl_lvl = (((bl_lvl & 0xff) << 8) | (bl_lvl >> 8));
	else
		inverted_dbv_bl_lvl = bl_lvl;

	if ((!strcmp(dsi_display->panel->oplus_priv.vendor_name, "RM692E5")) && (bl_lvl > 1)) {
		cmd_sets = &(dsi_display->panel->cur_mode->priv_info->cmd_sets[DSI_CMD_SET_BACKLIGHT]);
		cmds = &(cmd_sets->cmds[cmd_sets->count - 1]);
		msg = cmds->msg;
		tx_buf = (char*)msg.tx_buf;

		tx_buf[msg.tx_len-1] = (bl_lvl & 0xff);
		tx_buf[msg.tx_len-2] = (bl_lvl >> 8);

		rc = dsi_panel_tx_cmd_set(dsi_display->panel, DSI_CMD_SET_BACKLIGHT);
		if (rc < 0)
			DSI_ERR("Failed to set backlight:%d \n", bl_lvl);
		else
			return;
	}

	/* OPLUS_FEATURE_ADFR, backlight filter for OA */
	if (oplus_adfr_is_support()) {
		/* if backlight cmd is set after qsync window setting and qsync is enable, filter it
			otherwise tearing issue happen */
		if ((oplus_adfr_backlight_cmd_filter_get() == true) && (inverted_dbv_bl_lvl != 0)) {
			DSI_INFO("kVRR filter backlight cmd\n");
		} else {
			mutex_lock(&dsi_display->panel->panel_tx_lock);
#if defined(CONFIG_PXLW_IRIS)
			if (iris_is_chip_supported() && iris_is_pt_mode(dsi_display->panel))
				rc = iris_update_backlight(inverted_dbv_bl_lvl);
			else
#endif
				rc = mipi_dsi_dcs_set_display_brightness(dsi, inverted_dbv_bl_lvl);
			if (rc < 0)
				DSI_ERR("failed to update dcs backlight:%d\n", bl_lvl);
			mutex_unlock(&dsi_display->panel->panel_tx_lock);
		}
	} else {
#if defined(CONFIG_PXLW_IRIS)
		if (iris_is_chip_supported() && iris_is_pt_mode(dsi_display->panel))
			rc = iris_update_backlight(inverted_dbv_bl_lvl);
		else
#endif
			rc = mipi_dsi_dcs_set_display_brightness(dsi, inverted_dbv_bl_lvl);
		if (rc < 0)
			DSI_ERR("failed to update dcs backlight:%d\n", bl_lvl);
	}

	LCD_DEBUG_BACKLIGHT("Final backlight lvl:%d\n", bl_lvl);
	oplus_last_backlight = bl_lvl;
}

void oplus_display_backlight_notifier(struct dsi_display *display, u32 bl_lvl)
{
	enum panel_event_notifier_tag panel_type;
	u32 threshold = display->panel->bl_config.dc_backlight_threshold;
	bool *dc_mode = &display->panel->bl_config.oplus_dc_mode;

	panel_type = PANEL_EVENT_NOTIFICATION_PRIMARY;

	if (*dc_mode && (bl_lvl > 1 && bl_lvl < threshold)) {
		*dc_mode = false;
		oplus_display_event_data_notifier_trigger(display,
				panel_type,
				DRM_PANEL_EVENT_DC_MODE,
				*dc_mode);
	} else if (!(*dc_mode) && bl_lvl >= threshold) {
		*dc_mode = true;
		oplus_display_event_data_notifier_trigger(display,
				panel_type,
				DRM_PANEL_EVENT_DC_MODE,
				*dc_mode);
	}

	oplus_display_event_data_notifier_trigger(display,
			panel_type,
			DRM_PANEL_EVENT_BACKLIGHT,
			bl_lvl);
}
