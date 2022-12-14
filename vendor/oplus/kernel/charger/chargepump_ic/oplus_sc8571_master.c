/************************************************************************************
** File: msm_amss\adsp_proc\core\battman\battmngr\externalfg\src\oplus_sc8571_master.c
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
* Revision 1.0        2021-08-01         Kong Fanhong                  Created for PPS 125w
***********************************************************************/

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/proc_fs.h>

#include <trace/events/sched.h>
#include<linux/ktime.h>
#include "../oplus_vooc.h"
#include "../oplus_gauge.h"
#include "../oplus_charger.h"
#include "oplus_sc8571.h"
#include "../oplus_pps.h"


static struct chip_sc8571 *chip_sc8571_master = NULL;

static struct mutex i2c_rw_lock;
static bool error_reported = false;
extern void oplus_chg_sc8571_error(int report_flag, int *buf, int ret);

bool sc8571_master_get_enable(void);

/************************************************************************/
static void sc8571_i2c_error(bool happen)
{
	int report_flag = 0;
	struct oplus_pps_chip *chip = oplus_pps_get_pps_chip();
	if (!chip)
		return;

	if (error_reported)
		return;

	if (happen) {
		chip->pps_iic_err = 1;
		chip->pps_iic_err_num++;
		report_flag |= (1 << PPS_REPORT_ERROR_MASTER_I2C);
		oplus_chg_sc8571_error(report_flag, NULL, 0);
		if (chip->pps_iic_err_num >= 10) {
			error_reported = true;
		}
	}
}

static int __sc8571_read_byte(u8 reg, u8 *data)
{
	int ret;
	struct chip_sc8571 *chip = chip_sc8571_master;

	ret = i2c_smbus_read_byte_data(chip->master_client, reg);

	if (ret < 0) {
		sc8571_i2c_error(true);
		pr_err("i2c read fail: can't read from reg 0x%02X\n", reg);
		oplus_pps_notify_master_cp_error();
		return ret;
	}

	*data = (u8) ret;

	return 0;
}

static int __sc8571_write_byte(int reg, u8 val)
{
	int ret;
	struct chip_sc8571 *chip = chip_sc8571_master;


	ret = i2c_smbus_write_byte_data(chip->master_client, reg, val);
	if (ret < 0) {
		sc8571_i2c_error(true);
		pr_err("i2c write fail: can't write 0x%02X to reg 0x%02X: %d\n",
		       val, reg, ret);
		oplus_pps_notify_master_cp_error();
		return ret;
	}

	return 0;
}

static int sc8571_read_byte(u8 reg, u8 *data)
{
	int ret;

	mutex_lock(&i2c_rw_lock);
	ret = __sc8571_read_byte(reg, data);
	mutex_unlock(&i2c_rw_lock);

	return ret;
}

static int sc8571_write_byte(u8 reg, u8 data)
{
	int ret;

	mutex_lock(&i2c_rw_lock);
	ret = __sc8571_write_byte(reg, data);
	mutex_unlock(&i2c_rw_lock);

	return ret;
}



static int sc8571_read_word(u8 reg, u8 *data_block)
{
	struct chip_sc8571 *chip = chip_sc8571_master;
	int ret;

	mutex_lock(&i2c_rw_lock);
	ret = i2c_smbus_read_i2c_block_data(chip->master_client, reg, 2, data_block);
	if (ret < 0) {
		sc8571_i2c_error(true);
		chg_err("i2c read word fail: can't read reg:0x%02X \n", reg);
		mutex_unlock(&i2c_rw_lock);
		oplus_pps_notify_master_cp_error();
		return ret;
	}
	mutex_unlock(&i2c_rw_lock);
	return ret;
}

static int sc8571_master_i2c_masked_write(u8 reg, u8 mask, u8 val)
{
	int ret;
	u8 tmp;

	mutex_lock(&i2c_rw_lock);
	ret = __sc8571_read_byte(reg, &tmp);
	if (ret) {
		pr_err("Failed: reg=%02X, ret=%d\n", reg, ret);
		goto out;
	}

	tmp &= ~mask;
	tmp |= val & mask;

	ret = __sc8571_write_byte(reg, tmp);
	if (ret)
		pr_err("Faileds: reg=%02X, ret=%d\n", reg, ret);
out:
	mutex_unlock(&i2c_rw_lock);
	return ret;
}

int sc8571_master_get_tdie(void)
{
	u8 data_block[2] = {0};
	int tdie = 0;

	sc8571_read_word(SC8571_REG_37, data_block);
	tdie = (((data_block[0] & SC8571_TDIE_POL_H_MASK) << 8) | (data_block[1] & SC8571_TDIE_POL_L_MASK))*SC8571_TDIE_ADC_LSB;
	pps_err("0x37[0x%x] 0x38[0x%x] tdie[%d]\n", data_block[0], data_block[1], tdie);

	return tdie;
}

int sc8571_master_get_ucp_flag(void)
{
	int ret = 0;
	u8 temp;
	int ucp_fail = 0;


	ret = sc8571_read_byte(SC8571_REG_19, &temp);
	if (ret < 0) {
		pr_err("SC8571_REG_19\n");
		return 0;
	}

	ucp_fail =(temp & SC8571_BUS_UCP_FALL_FLAG_MASK) >> SC8571_BUS_UCP_FALL_FLAG_SHIFT;
	pps_err("0x19[0x%x] ucp_fail = %d\n", temp, ucp_fail);
	/*
	if(ucp_fail == 1) {
		sc8571_master_dump_registers();
		sc8571_slave_dump_registers();
	}*/


	return ucp_fail;
}

int sc8571_master_get_int_value(void)
{
	int ret = 0;
	u8 addr, int_column[PPS_DUMP_REG_CNT];

	struct oplus_pps_chip *chip = oplus_pps_get_pps_chip();
	if (!chip)
		return -1;

	for (addr = SC8571_REG_18; addr <= SC8571_REG_1C; addr++) {
		ret = sc8571_read_byte(addr, &int_column[addr-SC8571_REG_18]);
		if (ret < 0) {
			pps_err("sc8571_master_get_int_value Couldn't read 0x%02x ret = %d\n", addr, ret);
			return -1;
		}
		pps_err("reg[0x%x] = 0x%x\n", addr, int_column[addr-SC8571_REG_18]);
	}
	addr = SC8571_REG_1C - SC8571_REG_18 + 1;
	ret = sc8571_read_byte(SC8571_REG_42, &int_column[addr]);
	memcpy(chip->int_column, int_column, sizeof(chip->int_column));
	oplus_pps_print_dbg_info(chip);
	return 0;
}

int sc8571_master_get_vout(void)
{
	u8 data_block[2] = {0};
	int vout = 0;

	sc8571_read_word(SC8571_REG_2D, data_block);
	vout = (((data_block[0] & SC8571_VOUT_POL_H_MASK) << 8) | (data_block[1] & SC8571_VOUT_POL_L_MASK))*SC8571_VOUT_ADC_LSB;

	return vout;
}

int sc8571_master_get_vac(void)
{
	u8 data_block[2] = {0};
	int vac;

	sc8571_read_word(SC8571_REG_29, data_block);
	vac = (((data_block[0] & SC8571_VAC1_POL_H_MASK) << 8) | (data_block[1] & SC8571_VAC1_POL_L_MASK))*SC8571_VAC1_ADC_LSB;

	return vac;
}

int sc8571_master_get_vbus(void)
{
	u8 data_block[2] = {0};
	int cp_vbus;

	sc8571_read_word(SC8571_REG_27, data_block);
	cp_vbus = (((data_block[0] & SC8571_VBUS_POL_H_MASK) << 8) | (data_block[1] & SC8571_VBUS_POL_L_MASK))*SC8571_VBUS_ADC_LSB;

	return cp_vbus;
}

int sc8571_master_get_ibus(void)
{
	u8 data_block[2] = {0};
	int cp_ibus;

	sc8571_master_get_tdie();
	sc8571_read_word(SC8571_REG_25, data_block);
	cp_ibus = (((data_block[0] & SC8571_IBUS_POL_H_MASK) << 8) | (data_block[1] & SC8571_IBUS_POL_L_MASK))*SC8571_IBUS_ADC_LSB;

	return cp_ibus;
}


int sc8571_master_cp_enable(int enable)
{
	struct chip_sc8571 *sc8571 = chip_sc8571_master;
	int ret = 0;
	if (!sc8571) {
		pps_err("Failed\n");
		return -1;
	}
	pps_err(" enable = %d\n", enable);
	if (enable && (sc8571_master_get_enable() == false)) {
		ret = sc8571_master_i2c_masked_write(SC8571_REG_0F, SC8571_CHG_EN_MASK, SC8571_CHG_ENABLE << SC8571_CHG_EN_SHIFT);
	}
	else if (!enable && (sc8571_master_get_enable() == true)) {
		ret = sc8571_master_i2c_masked_write(SC8571_REG_0F, SC8571_CHG_EN_MASK, SC8571_CHG_DISABLE << SC8571_CHG_EN_SHIFT);
	}
	return ret;
}

bool sc8571_master_get_enable(void)
{
	int ret = 0;
	u8 temp;
	bool cp_enable = false;

	ret = sc8571_read_byte(SC8571_REG_0F, &temp);
	if (ret < 0) {
		pr_err("SC8571_REG_0F\n");
		return -1;
	}

	cp_enable =(temp & SC8571_CHG_EN_MASK) >> SC8571_CHG_EN_SHIFT;

	return cp_enable;
}

void sc8571_master_pmid2vout_enable(bool enable)
{
	return;/*return temporary*/
	if (enable == false)
		sc8571_write_byte(SC8571_REG_41, 0x20);/*0X41 disable pmid2vout*/
	else
		sc8571_write_byte(SC8571_REG_41, 0x00);/*0X41 enable pmid2vout*/
}

void sc8571_master_cfg_sc(void)
{
	sc8571_write_byte(SC8571_REG_0F, 0x00);/*0x0F Disable charge, SC_mode*/
	sc8571_write_byte(SC8571_REG_00, 0x7F);/*0X00	EN_BATOVP=9.540V*/
	sc8571_write_byte(SC8571_REG_01, 0xC6);/*0X01 DIS_BATOVP_ALM*/
	sc8571_write_byte(SC8571_REG_02, 0xD1);/*0X02 DIS_BATOCP*/
	sc8571_write_byte(SC8571_REG_03, 0xD0);/*0X03 DIS_BATOCP_ALM*/

	sc8571_write_byte(SC8571_REG_05, 0x00);/*0X05 DIS_BATOCP_ALM*/
	sc8571_write_byte(SC8571_REG_06, 0x4B);/*0X06 BUS_OVP=23V*/
	sc8571_write_byte(SC8571_REG_07, 0xA2);/*0X07 DIS_BUSOVP_ALM*/
	sc8571_write_byte(SC8571_REG_08, 0x0E);/*0X08 DIS_BUSOVP_ALM*/
	sc8571_write_byte(SC8571_REG_0A, 0x0C);/*0X0A TDIE_FLT=140. TDIE_ALM enable.  DIS_TDIE_ALM. DIS_TSBUS. DIS_TSBAT*/

	sc8571_write_byte(SC8571_REG_0E, 0xD8);/*0X0E VAC1OVP=12V. VAC2OVP=22V*/

	sc8571_write_byte(SC8571_REG_10, 0x30);/*0X10 disalbe watchdog*/
	sc8571_write_byte(SC8571_REG_11, 0x58);/*0X11  IBUS UCP*/
	sc8571_write_byte(SC8571_REG_12, 0x60);/*0X12 DIS_BATOCP*/
	sc8571_write_byte(SC8571_REG_23, 0x80);/*0X23 DIS_BATOCP_ALM*/
	sc8571_write_byte(SC8571_REG_24, 0x0E);/*0X24 DIS_BATOCP_ALM*/
	sc8571_write_byte(SC8571_REG_42, 0x5C);/*0X42 pmid2vout 400mv*/
	sc8571_master_pmid2vout_enable(false);

	pps_err("end!\n");
}

void sc8571_master_cfg_bypass(void)
{
	sc8571_write_byte(SC8571_REG_0F, 0x08);/*0x0F Disable charge, Bypass_mode, EN_ACDRV1*/
	sc8571_write_byte(SC8571_REG_00, 0x7F);/*0X00	EN_BATOVP=9.540V*/
	sc8571_write_byte(SC8571_REG_01, 0xC6);/*0X01 DIS_BATOVP_ALM*/
	sc8571_write_byte(SC8571_REG_02, 0xD1);/*0X02 DIS_BATOCP*/
	sc8571_write_byte(SC8571_REG_03, 0xD0);/*0X03 DIS_BATOCP_ALM*/
	sc8571_write_byte(SC8571_REG_05, 0x0);/*0X05 DIS_BATOCP_ALM*/

	sc8571_write_byte(SC8571_REG_06, 0x5A);/*0X06 BUS_OVP=10.5V*/
	sc8571_write_byte(SC8571_REG_07, 0xA2);/*0X07 DIS_BUSOVP_ALM*/
	sc8571_write_byte(SC8571_REG_08, 0x1C);/*0X08 DIS_BUSOVP_ALM*/
	sc8571_write_byte(SC8571_REG_0A, 0x0C);/*0X0A TDIE_FLT=140. TDIE_ALM enable.  DIS_TDIE_ALM. DIS_TSBUS. DIS_TSBAT*/
	sc8571_write_byte(SC8571_REG_0E, 0x58);/*0X0E VAC1OVP=12V. VAC2OVP=22V*/

	sc8571_write_byte(SC8571_REG_10, 0x84);/*0X10 disalbe watchdog*/
	sc8571_write_byte(SC8571_REG_11, 0x58);/*0X11 DIS_BATOVP_ALM*/
	sc8571_write_byte(SC8571_REG_12, 0x60);/*0X12 DIS_BATOCP*/
	sc8571_write_byte(SC8571_REG_23, 0x80);/*0X23 DIS_BATOCP_ALM*/
	sc8571_write_byte(SC8571_REG_24, 0x0E);/*0X24 DIS_BATOCP_ALM*/
	sc8571_write_byte(SC8571_REG_42, 0xFC);/*0xFC*/
	pps_err(" end! \n");
}

void sc8571_master_hardware_init(void)
{
	sc8571_write_byte(SC8571_REG_0F, 0x08);/*0x0F Disable charge, Bypass_mode, EN_ACDRV1*/
	sc8571_write_byte(SC8571_REG_00, 0x7F);/*0X00	EN_BATOVP=9.540V*/
	sc8571_write_byte(SC8571_REG_01, 0xC6);/*0X01 DIS_BATOVP_ALM*/
	sc8571_write_byte(SC8571_REG_02, 0xD1);/*0X02 DIS_BATOCP*/
	sc8571_write_byte(SC8571_REG_03, 0xD0);/*0X03 DIS_BATOCP_ALM*/
	sc8571_write_byte(SC8571_REG_06, 0x46);/*0X06 BUS_OVP=10.5V*/
	sc8571_write_byte(SC8571_REG_07, 0xA2);/*0X07 DIS_BUSOVP_ALM*/
	sc8571_write_byte(SC8571_REG_0A, 0x0C);/*0X0A TDIE_FLT=140. TDIE_ALM enable.  DIS_TDIE_ALM. DIS_TSBUS. DIS_TSBAT*/

	sc8571_write_byte(SC8571_REG_0E, 0x58);/*0X0E VAC1OVP=12V. VAC2OVP=22V*/
	sc8571_write_byte(SC8571_REG_10, 0x84);/*0X10 disalbe watchdog*/
	sc8571_write_byte(SC8571_REG_23, 0x00);/*0X23 adc disable continous*/
	sc8571_write_byte(SC8571_REG_24, 0x0E);/*0X24 disalbe TSBUT_ADC/TSBAT_ADC/IBAT_ADC*/
	pps_err(" end!\n");
}

void sc8571_master_reset(void)
{
	sc8571_write_byte(SC8571_REG_0F, 0x80);/*0x0F reset cp*/
	pps_err(" end!\n");
}

int sc8571_master_dump_registers(void)
{
	int ret = 0;

	u8 addr;
	u8 val_buf[11] = {0x0};

	for (addr = SC8571_REG_13; addr <= SC8571_REG_1C; addr++) {
		ret = sc8571_read_byte(addr, &val_buf[addr-SC8571_REG_13]);
		if (ret < 0) {
			pps_err("sc8571_master_dump_registers Couldn't read 0x%02x ret = %d\n", addr, ret);
			return -1;
		}
	}
	ret = sc8571_read_byte(SC8571_REG_42, &val_buf[10]);
	if (ret < 0) {
		pps_err("SC8571_REG_19 fail\n");
		return -1;
	}

	pps_err("sc8571_master_dump_registers:[13~17][0x%x, 0x%x, 0x%x, 0x%x, 0x%x]\n",
		val_buf[0], val_buf[1], val_buf[2], val_buf[3], val_buf[4]);
	pps_err("sc8571_master_dump_registers:[18~1c][0x%x, 0x%x, 0x%x, 0x%x, 0x%x][0x42= 0x%x]\n",
		val_buf[5], val_buf[6], val_buf[7], val_buf[8], val_buf[9], val_buf[10]);
	return ret;
}

static ssize_t sc8571_show_registers(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
	u8 addr;
	u8 val;
	u8 tmpbuf[300];
	int len;
	int idx = 0;
	int ret;

	idx = snprintf(buf, PAGE_SIZE, "%s:\n", "sc8571");
	for (addr = SC8571_REG_00; addr <= SC8571_REG_43; addr++) {
		ret = sc8571_read_byte(addr, &val);
		if (ret == 0) {
			len = snprintf(tmpbuf, PAGE_SIZE - idx, "Reg[%.2X] = 0x%.2x\n", addr, val);
			memcpy(&buf[idx], tmpbuf, len);
			idx += len;
		}
	}

	return idx;
}

static ssize_t sc8571_store_register(struct device *dev,
                                     struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned int reg;
	unsigned int val;

	ret = sscanf(buf, "%x %x", &reg, &val);
	if (ret == 2 && reg <= SC8571_REG_43)
		sc8571_write_byte((unsigned char)reg, (unsigned char)val);

	return count;
}
static DEVICE_ATTR(registers, 0660, sc8571_show_registers, sc8571_store_register);

static void sc8571_create_device_node(struct device *dev)
{
	device_create_file(dev, &dev_attr_registers);
}

static int sc8571_parse_dt(struct chip_sc8571 *chip)
{
	if (!chip) {
		pr_debug("chip null\n");
		return -1;
	}

	return 0;
}

irqreturn_t sc8571_ucp_interrupt_handler(struct chip_sc8571 *chip)
{
	int ucp_value;
	if(!oplus_pps_get_pps_mos_started()) {
		chg_err(",oplus_pps_get_pps_mos_started false\r\n");
		return IRQ_HANDLED;
	}
	/*change to work*/
	sc8571_master_get_int_value();
	ucp_value = sc8571_master_get_ucp_flag();

	if(ucp_value) {
		oplus_pps_stop_disconnect();
	}
	chg_err(",ucp_value = %d", ucp_value);

	return IRQ_HANDLED;
}

static int sc8571_irq_gpio_init(struct chip_sc8571 *chip)
{
	int rc;
	struct device_node *node = chip->master_dev->of_node;

	if (!node) {
		chg_err("device tree node missing\n");
		return -EINVAL;
	}

	chip->ucp_gpio = of_get_named_gpio(node,
	                                   "qcom,ucp_gpio", 0);
	if (chip->ucp_gpio < 0) {
		chg_err("chip->irq_gpio not specified\n");
	} else {
		if (gpio_is_valid(chip->ucp_gpio)) {
			rc = gpio_request(chip->ucp_gpio,
			                  "ucp_gpio");
			if (rc) {
				chg_err("unable to request gpio [%d]\n",
				         chip->ucp_gpio);
			}
		}
		chg_err("chip->ucp_gpio =%d\n", chip->ucp_gpio);
		chip->ucp_irq = gpio_to_irq(chip->ucp_gpio);
		chg_err("irq chip->ucp_irq =%d\n", chip->ucp_irq);
	}

	/* set voocphy pinctrl*/
	chip->ucp_pinctrl = devm_pinctrl_get(chip->master_dev);
	if (IS_ERR_OR_NULL(chip->ucp_pinctrl)) {
		chg_err("get pinctrl fail\n");
		return -EINVAL;
	}

	chip->ucp_int_active =
	    pinctrl_lookup_state(chip->ucp_pinctrl, "ucp_int_active");
	if (IS_ERR_OR_NULL(chip->ucp_int_active)) {
		chg_err(": %d Failed to get the state pinctrl handle\n", __LINE__);
		return -EINVAL;
	}

	chip->ucp_int_sleep =
	    pinctrl_lookup_state(chip->ucp_pinctrl, "ucp_int_sleep");
	if (IS_ERR_OR_NULL(chip->ucp_int_sleep)) {
		chg_err(": %d Failed to get the state pinctrl handle\n", __LINE__);
		return -EINVAL;
	}

	gpio_direction_input(chip->ucp_gpio);
	pinctrl_select_state(chip->ucp_pinctrl, chip->ucp_int_active); /* no_PULL */

	rc = gpio_get_value(chip->ucp_gpio);
	chg_err("irq chip->ucp_gpio input =%d irq_gpio_stat = %d\n", chip->ucp_gpio, rc);

	return 0;
}

static irqreturn_t sc8571_ucp_interrupt(int irq, void *dev_id)
{
	struct chip_sc8571 *chip = dev_id;

	if (!chip) {
		return IRQ_HANDLED;
	}

	return sc8571_ucp_interrupt_handler(chip);
}

static int sc8571_irq_register(struct chip_sc8571 *chip)
{
	int ret = 0;

	sc8571_irq_gpio_init(chip);
	chg_err("sc8571 chip->ucp_irq = %d\n", chip->ucp_irq);
	if (chip->ucp_irq) {
		ret = request_threaded_irq(chip->ucp_irq, NULL,
		                           sc8571_ucp_interrupt,
		                           IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
		                           "sc8571_ucp_irq", chip);
		if (ret < 0) {
			chg_err("request irq for ucp_irq=%d failed, ret =%d\n",
			         chip->ucp_irq, ret);
			return ret;
		}
		enable_irq_wake(chip->ucp_irq);
	}
	chg_err("request irq ok\n");

	return ret;
}

extern int pps_cp_id;
int sc8571_master_get_hw_id(void)
{
	u8 temp;
	int hw_id = PPS_CP_ID_SC8571;
	int ret;

	ret = sc8571_read_byte(SC8571_REG_22, &temp);
	if (ret < 0) {
		pps_err("failed sc8571_master_get_enable\n");
		return hw_id;
	}
	hw_id = (int)temp;
	pps_err("sc8571_master_get_hw_id--temp = 0x%x\n", hw_id);
	return hw_id;
}

static void register_pps_devinfo(void)
{
	int ret = 0;
	char *version;
	char *manufacture;

	version = "sc8571";
	manufacture = "MP";

	ret = register_device_proc("sc8571", version, manufacture);
	if (ret)
		chg_err("register_pps_devinfo fail\n");
}

static int sc8571_master_probe(struct i2c_client *client,
                                const struct i2c_device_id *id)
{
	struct chip_sc8571 *chip;

	pps_err(" enter!\n");

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip) {
		dev_err(&client->dev, "Couldn't allocate memory\n");
		return -ENOMEM;
	}

	chip->master_client = client;
	chip->master_dev = &client->dev;
	mutex_init(&i2c_rw_lock);

	i2c_set_clientdata(client, chip);
	chip_sc8571_master = chip;
	if(sc8571_master_get_hw_id() == PPS_CP_ID_BQ25980) {
		pps_cp_id = PPS_CP_ID_BQ25980;
		return 0;
	}
	else {
		pps_cp_id = PPS_CP_ID_SC8571;
	}

	sc8571_create_device_node(&(client->dev));

	sc8571_parse_dt(chip);

	sc8571_master_hardware_init();
	/*oplus_pps_cp_register_ops(&oplus_sc8571_ops);*/
	sc8571_irq_register(chip);

	sc8571_master_get_enable();

	register_pps_devinfo();

	pps_err("sc8571_parse_dt successfully!\n");

	return 0;
}

static void sc8571_master_shutdown(struct i2c_client *client)
{
	return;
}




static struct of_device_id sc8571_master_match_table[] = {
	{
		.compatible = "oplus,sc8571-master",
	},
	{},
};

static const struct i2c_device_id sc8571_master_charger_id[] = {
	{"sc8571-master", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, sc8571_master_charger_id);

static struct i2c_driver sc8571_master_driver = {
	.driver		= {
		.name	= "sc8571-master",
		.owner	= THIS_MODULE,
		.of_match_table = sc8571_master_match_table,
	},
	.id_table	= sc8571_master_charger_id,

	.probe		= sc8571_master_probe,
	.shutdown	= sc8571_master_shutdown,
};

#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 4, 0))
static int __init sc8571_master_subsys_init(void)
{
	int ret = 0;
	pps_err("init start\n");

	if (i2c_add_driver(&sc8571_master_driver) != 0) {
		pps_err(" failed to register sc8571 i2c driver.\n");
	} else {
		pps_err(" Success to register sc8571 i2c driver.\n");
	}

	return ret;
}
subsys_initcall(sc8571_master_subsys_init);
#else
int sc8571_master_subsys_init(void)
{
	int ret = 0;
	pps_err("2 init start\n");

	if (i2c_add_driver(&sc8571_master_driver) != 0) {
		pps_err(" failed to register sc8571 i2c driver.\n");
	} else {
		pps_err(" Success to register sc8571 i2c driver.\n");
	}

	return ret;
}

void sc8571_master_subsys_exit(void)
{
	i2c_del_driver(&sc8571_master_driver);
}
#endif


MODULE_DESCRIPTION("SC SC8571 Master Charge Pump Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("kongfanhong@oplus.com");
