/***************************************************************
** Copyright (C),  2021,  oplus Mobile Comm Corp.,  Ltd
**
** File : oplus_display_esd.h
** Description : oplus esd feature
** Version : 1.0
******************************************************************/
#ifndef _OPLUS_ESD_H_
#define _OPLUS_ESD_H_

#include "dsi_panel.h"
#include "dsi_defs.h"
#include "oplus_display_private_api.h"

int oplus_panel_parse_esd_reg_read_configs(struct dsi_panel *panel);
bool oplus_display_validate_reg_read(struct dsi_panel *panel);

#endif /* _OPLUS_ESD_H_ */
