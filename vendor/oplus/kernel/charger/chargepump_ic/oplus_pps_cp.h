/************************************************************************************
** File: msm_amss\adsp_proc\core\battman\battmngr\externalfg\src\oplus_oplus_cp.h
** OPLUS_FEATURE_CHG_BASIC
** Copyright (C), 2020-2022, OPLUS Mobile Comm Corp., Ltd
**
** Description:
**          for PPS 125w
**
** Version: 1.0
** Date created: 2021-08-01
** Author: Kong Fanhong
**
** --------------------------- Revision History: -----------------------------------
* <version>           <date>              <author>                           <desc>
* Revision 1.0        2021-08-01         Kong Fanhong                  Created for PPS 150w
***********************************************************************/

#ifndef _OPLUS_PPS_CP_H_
#define _OPLUS_PPS_CP_H_
int oplus_pps_cp_init(void);
void oplus_pps_cp_deinit(void);
int oplus_cp_master_dump_registers(void);
void oplus_cp_master_hardware_init(void);
void oplus_cp_master_cfg_bypass(void);
void oplus_cp_master_cfg_sc(void);
int oplus_cp_master_cp_enable(int enable);
bool oplus_cp_master_get_enable(void);
int oplus_cp_master_get_ibus(void);
int oplus_cp_master_get_vbus(void);
int oplus_cp_master_get_vac(void);
int oplus_cp_master_get_vout(void);
int oplus_cp_master_get_tdie(void);
int oplus_cp_master_get_ucp_flag(void);
void oplus_cp_pmid2vout_enable(bool enable);

int oplus_cp_slave_dump_registers(void);
void oplus_cp_slave_hardware_init(void);
void oplus_cp_slave_cfg_bypass(void);
void oplus_cp_slave_cfg_sc(void);
int oplus_cp_slave_cp_enable(int enable);
bool oplus_cp_slave_get_enable(void);
int oplus_cp_slave_get_ibus(void);
int oplus_cp_slave_get_vbus(void);
int oplus_cp_slave_get_vac(void);
int oplus_cp_slave_get_vout(void);
int oplus_cp_slave_get_tdie(void);
int oplus_cp_slave_get_ucp_flag(void);

void oplus_cp_hardware_init(void);
int oplus_pps_get_authenticate(void);
void oplus_cp_reset(void);
int oplus_cp_cfg_mode_init(int mode);
#endif /*_OPLUS_PPS_CP_H_*/
