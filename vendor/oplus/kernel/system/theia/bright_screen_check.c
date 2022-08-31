// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */
#include <linux/version.h>
#include "powerkey_monitor.h"
#include "theia_kevent_kernel.h"

#define BRIGHTSCREEN_COUNT_FILE    "/data/oplus/log/bsp/brightscreen_count.txt"
#define BRIGHT_MAX_WRITE_NUMBER            50
#define BRIGHT_SLOW_TIMEOUT_MS            20000

/*Only CONFIG_DRM_PANEL_NOTIFY seperates BLANK event and UNBLANK event*/
#if IS_ENABLED (CONFIG_DRM_PANEL_NOTIFY)
#define THEIA_TOUCHPANEL_BLANK_EVENT              DRM_PANEL_EVENT_BLANK
#define THEIA_TOUCHPANEL_EARLY_BLANK_EVENT        DRM_PANEL_EVENT_BLANK_LP
#define THEIA_TOUCHPANEL_UNBLANK_EVENT            DRM_PANEL_EVENT_UNBLANK
#elif IS_ENABLED (CONFIG_DRM_MSM)
#define THEIA_TOUCHPANEL_BLANK_EVENT              MSM_DRM_EVENT_BLANK
#define THEIA_TOUCHPANEL_EARLY_BLANK_EVENT        MSM_DRM_EARLY_EVENT_BLANK
#define THEIA_TOUCHPANEL_UNBLANK_EVENT            MSM_DRM_EVENT_BLANK
#else
#define THEIA_TOUCHPANEL_BLANK_EVENT              FB_EVENT_BLANK
#define THEIA_TOUCHPANEL_EARLY_BLANK_EVENT        FB_EARLY_EVENT_BLANK
#define THEIA_TOUCHPANEL_UNBLANK_EVENT            FB_EVENT_BLANK
#endif

#define BRIGHT_DEBUG_PRINTK(a, arg...)\
	do {\
		printk("[powerkey_monitor_bright]: " a, ##arg);\
	} while (0)

struct bright_data g_bright_data = {
	.is_panic = 0,
	.status = BRIGHT_STATUS_INIT,
#if IS_ENABLED(CONFIG_DRM_PANEL_NOTIFY)
	.blank = DRM_PANEL_EVENT_UNBLANK,
#elif IS_ENABLED (CONFIG_DRM_MSM)
	.blank = MSM_DRM_BLANK_UNBLANK,
#else
	.blank = FB_BLANK_UNBLANK,
#endif
	.timeout_ms = BRIGHT_SLOW_TIMEOUT_MS,
	.get_log = 1,
	.error_count = 0,
	.active_panel = NULL,
	.cookie = NULL,
};

/* if last stage in this array, skip */
static char bright_last_skip_block_stages[][64] = {
	{ "POWERKEY_interceptKeyBeforeQueueing" }, /* framework policy may not goto sleep when bright check, skip */
};

/* if contain stage in this array, skip */
static char bright_skip_stages[][64] = {
	{ "POWER_wakeUpInternal" }, /* quick press powerkey, power decide wakeup when bright check, skip */
	{ "POWERKEY_wakeUpFromPowerKey" }, /* quick press powerkey, power decide wakeup when bright check, skip */
	{ "CANCELED_" }, /* if CANCELED_ event write in bright check stage, skip */
};

static void bright_timer_func(struct timer_list *t);

static int br_start_check_systemid = -1;

int bright_screen_timer_restart(void)
{
	BRIGHT_DEBUG_PRINTK("bright_screen_timer_restart:blank = %d,status = %d\n", g_bright_data.blank, g_bright_data.status);

	if (g_bright_data.status != BRIGHT_STATUS_CHECK_ENABLE && g_bright_data.status != BRIGHT_STATUS_CHECK_DEBUG) {
		BRIGHT_DEBUG_PRINTK("bright_screen_timer_restart:g_bright_data.status = %d return\n", g_bright_data.status);
		return g_bright_data.status;
	}

	if (!phx_is_system_boot_completed())
		return -1;

#if IS_ENABLED(CONFIG_DRM_PANEL_NOTIFY)
	if (g_bright_data.blank == DRM_PANEL_EVENT_UNBLANK) {  /*DRM_PANEL_EVENT_UNBLANK*/
#elif IS_ENABLED (CONFIG_DRM_MSM)
	if (g_bright_data.blank == MSM_DRM_BLANK_UNBLANK) {    /*MSM_DRM_BLANK_POWERDOWN*/
#else
	if (g_bright_data.blank == FB_BLANK_UNBLANK) {         /*FB_BLANK_POWERDOWN*/
#endif
		br_start_check_systemid = get_systemserver_pid();
		mod_timer(&g_bright_data.timer, jiffies + msecs_to_jiffies(g_bright_data.timeout_ms));
		BRIGHT_DEBUG_PRINTK("bright_screen_timer_restart: bright screen check start %u\n", g_bright_data.timeout_ms);
		theia_pwk_stage_start("POWERKEY_START_BR");
		return 0;
	}
	return g_bright_data.blank;
}
EXPORT_SYMBOL(bright_screen_timer_restart);

static int bright_write_error_count(struct bright_data *bri_data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	return 0;
#else
	struct file *fp;
	loff_t pos;
	ssize_t len = 0;
	char buf[256] = {'\0'};

	fp = filp_open(BRIGHTSCREEN_COUNT_FILE, O_RDWR | O_CREAT, 0664);
	if (IS_ERR(fp)) {
		BRIGHT_DEBUG_PRINTK("create %s file error fp:%p\n", BRIGHTSCREEN_COUNT_FILE, fp);
		return -1;
	}

	sprintf(buf, "%d\n", bri_data->error_count);

	pos = 0;
	len = kernel_write(fp, buf, strlen(buf), &pos);
	if (len < 0)
		BRIGHT_DEBUG_PRINTK("write %s file error\n", BRIGHTSCREEN_COUNT_FILE);

	pos = 0;
	kernel_read(fp, buf, sizeof(buf), &pos);
	BRIGHT_DEBUG_PRINTK("bright_write_error_count %s\n", buf);

	filp_close(fp, NULL);

	return len;
#endif
}

/* copy mtk_boot_common.h */
#define NORMAL_BOOT 0
#define ALARM_BOOT 7

static int get_status(void)
{
#ifdef CONFIG_DRM_MSM
	if (MSM_BOOT_MODE__NORMAL == get_boot_mode()) {
		return g_bright_data.status;
	}
	return BRIGHT_STATUS_INIT_SUCCEES;
#else
	if ((get_boot_mode() == NORMAL_BOOT) || (get_boot_mode() == ALARM_BOOT)) {
		return g_bright_data.status;
	}
	return BRIGHT_STATUS_INIT_SUCCEES;
#endif
}

static bool get_log_swich()
{
	return (BRIGHT_STATUS_CHECK_ENABLE == get_status() || BRIGHT_STATUS_CHECK_DEBUG == get_status()) && g_bright_data.get_log;
}

/*
logmap format:
logmap{key1:value1;key2:value2;key3:value3 ...}
*/
static void get_brightscreen_check_dcs_logmap(char *logmap)
{
	char stages[512] = {0};
	int stages_len;

	stages_len = get_pwkey_stages(stages);
	snprintf(logmap, 512, "logmap{logType:%s;error_id:%s;error_count:%u;systemserver_pid:%d;stages:%s;catchlog:%s}", PWKKEY_BRIGHT_SCREEN_DCS_LOGTYPE,
		g_bright_data.error_id, g_bright_data.error_count, get_systemserver_pid(), stages, get_log_swich() ? "true" : "false");
}

void send_bright_screen_dcs_msg(void)
{
	char logmap[512] = {0};
	get_brightscreen_check_dcs_logmap(logmap);
	SendDcsTheiaKevent(PWKKEY_DCS_TAG, PWKKEY_DCS_EVENTID, logmap);
}

static void dump_freeze_log(void)
{
	/* send kevent dcs msg */
	send_bright_screen_dcs_msg();
}

static bool is_bright_last_stage_skip()
{
	int i = 0, nLen;
	char stage[64] = {0};;
	get_last_pwkey_stage(stage);

	nLen = sizeof(bright_last_skip_block_stages)/sizeof(bright_last_skip_block_stages[0]);

	/* BRIGHT_DEBUG_PRINTK("is_bright_last_stage_skip stage:%s nLen:%d", stage, nLen); */
	for (i = 0; i < nLen; i++) {
		/* BRIGHT_DEBUG_PRINTK("is_bright_last_stage_skip stage:%s i:%d nLen:%d bright_last_skip_block_stages[i]:%s",
			stage, i, nLen, bright_last_skip_block_stages[i]); */
        if (!strcmp(stage, bright_last_skip_block_stages[i])) {
			BRIGHT_DEBUG_PRINTK("is_bright_last_stage_skip return true, stage:%s", stage);
			return true;
		}
	}

	return false;
}

static bool is_bright_contain_skip_stage()
{
	char stages[512] = {0};
	int i = 0, nArrayLen;
	get_pwkey_stages(stages);

	nArrayLen = sizeof(bright_skip_stages)/sizeof(bright_skip_stages[0]);
	for (i = 0; i < nArrayLen; i++) {
		if (strstr(stages, bright_skip_stages[i]) != NULL) {
			BRIGHT_DEBUG_PRINTK("is_bright_contain_skip_stage return true, stages:%s", stages);
			return true;
		}
	}

	return false;
}

static bool is_need_skip()
{
	if (is_bright_last_stage_skip()) {
		return true;
	}

	if (is_bright_contain_skip_stage()) {
		return true;
	}

	return false;
}

/* if the error id contain current pid, we think is a normal resume */
static bool is_normal_resume()
{
	char current_pid_str[32];
	sprintf(current_pid_str, "%d", get_systemserver_pid());
	if (!strncmp(g_bright_data.error_id, current_pid_str, strlen(current_pid_str))) {
		return true;
	}

	return false;
}

static void get_bright_resume_dcs_logmap(char *logmap)
{
	snprintf(logmap, 512, "logmap{logType:%s;error_id:%s;resume_count:%u;normalReborn:%s;catchlog:false}", PWKKEY_BRIGHT_SCREEN_DCS_LOGTYPE,
		g_bright_data.error_id, g_bright_data.error_count, (is_normal_resume() ? "true" : "false"));
}

static void send_bright_screen_resume_dcs_msg(void)
{
	/* check the current systemserver pid and the error_id, judge if it is a normal resume or reboot resume */
	char resume_logmap[512] = {0};
	get_bright_resume_dcs_logmap(resume_logmap);
	SendDcsTheiaKevent(PWKKEY_DCS_TAG, PWKKEY_DCS_EVENTID, resume_logmap);
}

static void delete_timer(char *reason, bool cancel)
{
	del_timer(&g_bright_data.timer);

	if (cancel && g_bright_data.error_count != 0) {
		send_bright_screen_resume_dcs_msg();
		g_bright_data.error_count = 0;
		sprintf(g_bright_data.error_id, "%s", "null");
	}

	theia_pwk_stage_end(reason);
}

static void bright_error_happen_work(struct work_struct *work)
{
	struct bright_data *bri_data = container_of(work, struct bright_data, error_happen_work);

	/* for bright screen check, check if need skip, we direct return */
	if (is_need_skip()) {
		return;
	}

	if (bri_data->error_count == 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
		struct timespec64 ts = { 0 };
		ktime_get_real_ts64(&ts);
		sprintf(g_bright_data.error_id, "%d.%lld.%ld", get_systemserver_pid(), ts.tv_sec, ts.tv_nsec);
#else
		struct timespec ts;
		getnstimeofday(&ts);
		sprintf(g_bright_data.error_id, "%d.%ld.%ld", get_systemserver_pid(), ts.tv_sec, ts.tv_nsec);
#endif
	}

	if (bri_data->error_count < BRIGHT_MAX_WRITE_NUMBER) {
		bri_data->error_count++;
		dump_freeze_log();
		bright_write_error_count(bri_data);
	}
	BRIGHT_DEBUG_PRINTK("bright_error_happen_work error_id = %s, error_count = %d\n",
		bri_data->error_id, bri_data->error_count);

	delete_timer("BL_ERROR_HAPPEN", false);

	if(bri_data->is_panic) {
		doPanic();
	}
}

static void bright_timer_func(struct timer_list *t)
{
	struct bright_data * p = from_timer(p, t, timer);

	BRIGHT_DEBUG_PRINTK("bright_timer_func is called\n");

#if IS_ENABLED(CONFIG_DRM_PANEL_NOTIFY)
	if (g_bright_data.active_panel == NULL || g_bright_data.cookie == NULL) {
		BRIGHT_DEBUG_PRINTK("br check register panel not ready\n");
		return;
	}
#endif

	if (br_start_check_systemid == get_systemserver_pid()) {
		schedule_work(&p->error_happen_work);
	} else {
		BRIGHT_DEBUG_PRINTK("bright_timer_func, not valid for check, skip\n");
	}
}

static int check_bright_screen_tp_event(int *blank, unsigned long event)
{
	if (!event) {
		BRIGHT_DEBUG_PRINTK("bright_fb_notifier_callback: Invalid event");
		return -1;
	}

	switch (event) {
	case THEIA_TOUCHPANEL_BLANK_EVENT:
	case THEIA_TOUCHPANEL_UNBLANK_EVENT:
	case THEIA_TOUCHPANEL_EARLY_BLANK_EVENT:
		g_bright_data.blank = *blank;
		if (g_bright_data.status != BRIGHT_STATUS_CHECK_DEBUG) {
			delete_timer("FINISH_FB", true);
			BRIGHT_DEBUG_PRINTK("bright_fb_notifier_callback: del timer,event:%lu status:%d blank:%d\n",
				event, g_bright_data.status, g_bright_data.blank);
		} else {
			BRIGHT_DEBUG_PRINTK("bright_fb_notifier_callback:event = %lu status:%d blank:%d\n",
				event, g_bright_data.status, g_bright_data.blank);
		}
		break;
	default:
		BRIGHT_DEBUG_PRINTK("bright_fb_notifier_callback:event = %lu status:%d blank:%d\n",
		event, g_bright_data.status, g_bright_data.blank);
		break;
	}
	return 0;
}

#if IS_ENABLED (CONFIG_DRM_PANEL_NOTIFY)
static void bright_fb_notifier_callback(enum panel_event_notifier_tag tag,
	struct panel_event_notification *notification, void *client_data)
{
	int blank_flag = 0;
	if (notification && notification -> notif_type) {
		blank_flag = notification -> notif_type;
		check_bright_screen_tp_event(&blank_flag, (unsigned long)(notification -> notif_type));
	}
}
#elif IS_ENABLED (CONFIG_DRM_MSM)
static int bright_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	int ret = 0;
	struct msm_drm_notifier *evdata = data;
	int *blank;

	if (evdata && evdata->data) {
		blank = evdata->data;
	}
	ret = check_bright_screen_tp_event(*blank, event);
	return ret;
}
#else
static int bright_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	int ret = 0;
	struct msm_drm_notifier *evdata = data;
	int *blank;

	if (evdata && evdata->data) {
		blank = evdata->data;
	}
	ret = check_bright_screen_tp_event(*blank, event);
	return ret;
}
#endif

static ssize_t bright_screen_cancel_proc_write(struct file *file, const char __user *buf,
	size_t count, loff_t *off)
{
	char buffer[40] = {0};
	char cancel_str[64] = {0};

	if (g_bright_data.status == BRIGHT_STATUS_INIT || g_bright_data.status == BRIGHT_STATUS_INIT_FAIL) {
		BRIGHT_DEBUG_PRINTK("%s init not finish: status = %d\n", __func__, g_bright_data.status);
		return count;
	}

	if (count >= 40) {
		count = 39;
	}

	if (copy_from_user(buffer, buf, count)) {
		BRIGHT_DEBUG_PRINTK("%s: read proc input error.\n", __func__);
		return count;
	}

	snprintf(cancel_str, sizeof(cancel_str), "CANCELED_BR_%s", buffer);
    delete_timer(cancel_str, true);

	return count;
}
static ssize_t bright_screen_cancel_proc_read(struct file *file, char __user *buf,
	size_t count, loff_t *off)
{
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int bright_screen_cancel_proc_show(struct seq_file *seq_file, void *data) {
	seq_printf(seq_file, "%s called\n", __func__);
	return 0;
}

static int bright_screen_cancel_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, bright_screen_cancel_proc_show, NULL);
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static const struct proc_ops bright_screen_cancel_proc_fops = {
	.proc_open = bright_screen_cancel_proc_open,
	.proc_read = bright_screen_cancel_proc_read,
	.proc_write = bright_screen_cancel_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
struct file_operations bright_screen_cancel_proc_fops = {
	.read = bright_screen_cancel_proc_read,
	.write = bright_screen_cancel_proc_write,
};
#endif

#if IS_ENABLED(CONFIG_DRM_PANEL_NOTIFY)
int br_register_panel_event_notify(void)
{
	void *data = NULL;
	void *cookie = NULL;

	cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
		PANEL_EVENT_NOTIFIER_CLIENT_PRIMARY_THEIA_BRIGHT,
		g_bright_data.active_panel, bright_fb_notifier_callback, data);

	if (!cookie) {
		BRIGHT_DEBUG_PRINTK("br_register_panel_event_notify failed\n");
		return -1;
	}
	g_bright_data.cookie = cookie;
	return 0;
}
#endif

void bright_screen_check_init(void)
{
#if IS_ENABLED (CONFIG_DRM_PANEL_NOTIFY)
	/* done in black_screen_check_init for reduce get_active_panel redundant code */
#elif IS_ENABLED (CONFIG_DRM_MSM)
	g_bright_data.fb_notif.notifier_call = bright_fb_notifier_callback;
	msm_drm_register_client(&g_bright_data.fb_notif);
#else
	g_bright_data.fb_notif.notifier_call = bright_fb_notifier_callback;
	fb_register_client(&g_bright_data.fb_notif);
#endif

	/* the node for cancel bright screen check */
	proc_create("brightSwitch", S_IRWXUGO, NULL, &bright_screen_cancel_proc_fops);

	INIT_WORK(&g_bright_data.error_happen_work, bright_error_happen_work);
	timer_setup((&g_bright_data.timer), (bright_timer_func), TIMER_DEFERRABLE);
	g_bright_data.status = BRIGHT_STATUS_CHECK_ENABLE;
}

void bright_screen_exit(void)
{
	delete_timer("FINISH_DRIVER_EXIT", true);
#if IS_ENABLED(CONFIG_DRM_PANEL_NOTIFY)
	if (g_bright_data.active_panel && g_bright_data.cookie)
		panel_event_notifier_unregister(g_bright_data.cookie);
#elif IS_ENABLED(CONFIG_DRM_MSM)
	msm_drm_unregister_client(&g_bright_data.fb_notif);
#else
	fb_unregister_client(&g_bright_data.fb_notif);
#endif
}
MODULE_LICENSE("GPL v2");

