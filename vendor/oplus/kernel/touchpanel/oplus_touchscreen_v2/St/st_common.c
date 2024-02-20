// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 oplus. All rights reserved.
 */

#include <linux/module.h>
#include "st_common.h"

#ifdef TPD_DEVICE
#undef TPD_DEVICE
#define TPD_DEVICE "st_common"
#else
#define TPD_DEVICE "st_common"
#endif
/*************************************auto test Funtion**************************************/

/*step1: read limit fw data*/
static int st_read_limit_fw(struct seq_file *s, struct touchpanel_data *ts,
			    struct auto_testdata *p_st_testdata)
{
	const struct firmware *fw = NULL;
	struct auto_test_header *test_head = NULL;

	TPD_INFO("%s: enter\n", __func__);
	fw = ts->com_test_data.limit_fw;
	/*step4: decode the limit image*/
	test_head = (struct auto_test_header *)fw->data;

	if ((test_head->magic1 != Limit_MagicNum1)
	    || (test_head->magic2 != Limit_MagicNum2)) {
		TPD_INFO("limit image is not generated by oplus\n");
		seq_printf(s, "limit image is not generated by oplus\n");
		return  -1;
	}

	TPD_INFO("current test item: %llx\n", test_head->test_item);
	/*init st*/
	p_st_testdata->tx_num = ts->hw_res.tx_num;
	p_st_testdata->rx_num = ts->hw_res.rx_num;
	p_st_testdata->irq_gpio = ts->hw_res.irq_gpio;
	p_st_testdata->tp_fw = ts->panel_data.tp_fw;
	p_st_testdata->fp = ts->com_test_data.result_data;
	p_st_testdata->length = ts->com_test_data.result_max_len;
	p_st_testdata->fw = fw;
	p_st_testdata->test_item = test_head->test_item;
	p_st_testdata->pos = &ts->com_test_data.result_cur_len;
	return 0;
}

/*step2: test support item*/
static int st_test_item(struct seq_file *s, struct touchpanel_data *ts,
			struct auto_testdata *p_st_testdata)
{
	int error_count = 0;
	int ret = 0;
	struct test_item_info *p_test_item_info = NULL;
	struct st_auto_test_operations *gd_test_ops = NULL;
	struct com_test_data *com_test_data_p = NULL;

	com_test_data_p = &ts->com_test_data;

	if (!com_test_data_p || !com_test_data_p->chip_test_ops) {
		TPD_INFO("%s: com_test_data is null\n", __func__);
		error_count++;
		goto END;
	}

	gd_test_ops = (struct st_auto_test_operations *)
		      com_test_data_p->chip_test_ops;

	if (!gd_test_ops->auto_test_preoperation) {
		TPD_INFO("not support gd_test_ops->auto_test_preoperation callback\n");

	} else {
		ret = gd_test_ops->auto_test_preoperation(s, ts->chip_data, p_st_testdata,
				p_test_item_info);

		if (ret < 0) {
			TPD_INFO("auto_test_preoperation failed\n");
			error_count++;
		}
	}

	p_test_item_info = get_test_item_info(p_st_testdata->fw, TYPE_TEST1);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST1);

	} else {
		ret = gd_test_ops->test1(s, ts->chip_data, p_st_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test%d failed! ret is %d\n", TYPE_TEST1, ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(p_st_testdata->fw, TYPE_TEST2);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST2);

	} else {
		ret = gd_test_ops->test2(s, ts->chip_data, p_st_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test%d failed! ret is %d\n", TYPE_TEST2, ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(p_st_testdata->fw, TYPE_TEST3);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST3);

	} else {
		ret = gd_test_ops->test3(s, ts->chip_data, p_st_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test%d failed! ret is %d\n", TYPE_TEST3, ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(p_st_testdata->fw, TYPE_TEST4);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST4);

	} else {
		ret = gd_test_ops->test4(s, ts->chip_data, p_st_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test%d failed! ret is %d\n", TYPE_TEST4, ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(p_st_testdata->fw, TYPE_TEST5);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST5);

	} else {
		ret = gd_test_ops->test5(s, ts->chip_data, p_st_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test%d failed! ret is %d\n", TYPE_TEST5, ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(p_st_testdata->fw, TYPE_TEST6);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST6);

	} else {
		ret = gd_test_ops->test6(s, ts->chip_data, p_st_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test%d failed! ret is %d\n", TYPE_TEST6, ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	p_test_item_info = get_test_item_info(p_st_testdata->fw, TYPE_TEST7);

	if (!p_test_item_info) {
		TPD_INFO("item: %d get_test_item_info fail\n", TYPE_TEST7);

	} else {
		ret = gd_test_ops->test7(s, ts->chip_data, p_st_testdata, p_test_item_info);

		if (ret < 0) {
			TPD_INFO("test%d failed! ret is %d\n", TYPE_TEST7, ret);
			error_count++;
		}
	}

	tp_kfree((void **)&p_test_item_info);

	if (!gd_test_ops->auto_test_endoperation) {
		TPD_INFO("not support gd_test_ops->auto_test_preoperation callback\n");

	} else {
		ret = gd_test_ops->auto_test_endoperation(s, ts->chip_data, p_st_testdata,
				p_test_item_info);

		if (ret < 0) {
			TPD_INFO("auto_test_endoperation failed\n");
			error_count++;
		}
	}

END:
	return error_count;
}

int st_auto_test(struct seq_file *s,  struct touchpanel_data *ts)
{
	struct auto_testdata st_testdata = {
		.tx_num = 0,
		.rx_num = 0,
		.fp = NULL,
		.pos = NULL,
		.irq_gpio = -1,
		.tp_fw = 0,
		.fw = NULL,
		.test_item = 0,
	};
	int error_count = 0;
	int ret = 0;

	ret = st_read_limit_fw(s, ts, &st_testdata);

	if (ret) {
		error_count++;
		goto END;
	}

	error_count += st_test_item(s, ts, &st_testdata);

END:
	seq_printf(s, "imageid = 0x%llx, deviceid = 0x%llx\n", st_testdata.tp_fw,
		   st_testdata.dev_tp_fw);
	seq_printf(s, "%d error(s). %s\n", error_count,
		   error_count ? "" : "All test passed.");
	TPD_INFO(" TP auto test %d error(s). %s\n", error_count,
		 error_count ? "" : "All test passed.");
	return error_count;
}
EXPORT_SYMBOL(st_auto_test);

int st_create_proc(struct touchpanel_data *ts,
		   struct st_proc_operations *st_ops)
{
	int ret = 0;


	return ret;
}
EXPORT_SYMBOL(st_create_proc);

int st_remove_proc(struct touchpanel_data *ts)
{
	if (!ts) {
		return -EINVAL;
	}


	return 0;
}
EXPORT_SYMBOL(st_remove_proc);

MODULE_DESCRIPTION("Touchscreen St Common Interface");
MODULE_LICENSE("GPL");
