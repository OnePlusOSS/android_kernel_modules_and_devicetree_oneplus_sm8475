/***************************************************************
** Copyright (C),  2021,  OPLUS Mobile Comm Corp.,  Ltd
**
** File : oplus_display_panel_oha.h
** Description : oplus display panel 1hz aod feature
** Version : 1.0
******************************************************************/
#ifndef _OPLUS_DISPLAY_PANEL_HZ_AOD_H_
#define _OPLUS_DISPLAY_PANEL_HZ_AOD_H_

#include <linux/err.h>
#include "dsi_display.h"
#include "dsi_panel.h"
#include "dsi_ctrl.h"
#include "dsi_ctrl_hw.h"
#include "dsi_drm.h"
#include "dsi_clk.h"
#include "dsi_pwr.h"
#include "sde_dbg.h"

enum oplus_oha_switch {
	OHA_SWITCH_OFF = 0,
	OHA_SWITCH_ON,
};

void oplus_oha_init(struct dsi_panel *panel);
inline bool oplus_oha_is_support(void);
int oplus_update_oha_mode_unlock(struct drm_crtc_state *cstate,
		struct dsi_panel *panel);
int oplus_display_update_oha_mode(struct drm_crtc_state *cstate,
		struct dsi_display *display);
int __oplus_display_panel_set_oha_mode(int mode);
int oplus_ofp_update_oha(struct drm_connector *connector);
int oplus_display_panel_update_oha_mode_unlock(struct dsi_panel *panel);
int oplus_display_panel_update_oha_mode(void);
bool sde_crtc_get_oha_mode(struct drm_crtc_state *crtc_state);
int oplus_display_panel_get_oha_enable(void *data);
int oplus_display_panel_set_oha_enable(void *data);
#endif /*_OPLUS_DISPLAY_PANEL_HZ_AOD_H_*/
