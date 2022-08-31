// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2018-2020 Oplus. All rights reserved.
 */

#include <linux/version.h>
#include "powerkey_monitor.h"
#include "theia_kevent_kernel.h"

#define BLACKSCREEN_COUNT_FILE    "/data/oplus/log/bsp/blackscreen_count.txt"
#define BLACK_MAX_WRITE_NUMBER            50
#define BLACK_SLOW_STATUS_TIMEOUT_MS    20000

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

#define BLACK_DEBUG_PRINTK(a, arg...)\
	do {\
		printk("[powerkey_monitor_black]: " a, ##arg);\
	} while (0)

static void black_timer_func(struct timer_list *t);

struct drm_panel *g_active_panel = NULL;
static struct delayed_work g_check_dt_work;
static int g_check_dt_retry_count;
#define CHECK_DT_DELAY_MS 15000

struct black_data g_black_data = {
	.is_panic = 0,
	.status = BLACK_STATUS_INIT,
#if IS_ENABLED (CONFIG_DRM_PANEL_NOTIFY)
	.blank = DRM_PANEL_EVENT_UNBLANK,
#elif IS_ENABLED (CONFIG_DRM_MSM)
	.blank = MSM_DRM_BLANK_UNBLANK,
#else
	.blank = FB_BLANK_UNBLANK,
#endif
	.timeout_ms = BLACK_SLOW_STATUS_TIMEOUT_MS,
	.get_log = 1,
	.error_count = 0,
	.active_panel = NULL,
	.cookie = NULL,
};

static int bl_start_check_systemid = -1;

int black_screen_timer_restart(void)
{
	BLACK_DEBUG_PRINTK("black_screen_timer_restart:blank = %d,status = %d\n", g_black_data.blank, g_black_data.status);

	if (g_black_data.status != BLACK_STATUS_CHECK_ENABLE && g_black_data.status != BLACK_STATUS_CHECK_DEBUG) {
		BLACK_DEBUG_PRINTK("black_screen_timer_restart:g_black_data.status = %d return\n", g_black_data.status);
		return g_black_data.status;
	}

	if (!phx_is_system_boot_completed())
		return -1;

#if IS_ENABLED (CONFIG_DRM_PANEL_NOTIFY)
	if (g_black_data.blank == DRM_PANEL_EVENT_BLANK) {
#elif IS_ENABLED (CONFIG_DRM_MSM)
	if (g_black_data.blank == MSM_DRM_BLANK_POWERDOWN) {
#else
	if (g_black_data.blank == FB_BLANK_POWERDOWN) {
#endif
		bl_start_check_systemid = get_systemserver_pid();
		mod_timer(&g_black_data.timer, jiffies + msecs_to_jiffies(g_black_data.timeout_ms));
		BLACK_DEBUG_PRINTK("black_screen_timer_restart: black screen check start %u\n", g_black_data.timeout_ms);
		theia_pwk_stage_start("POWERKEY_START_BL");
		return 0;
	}
	return g_black_data.blank;
}
EXPORT_SYMBOL(black_screen_timer_restart);

/* copy mtk_boot_common.h */
#define NORMAL_BOOT 0
#define ALARM_BOOT 7
static int get_status(void)
{
#ifdef CONFIG_DRM_MSM
	if (MSM_BOOT_MODE__NORMAL == get_boot_mode()) {
		return g_black_data.status;
	}
	return BLACK_STATUS_INIT_SUCCEES;
#else
	if ((get_boot_mode() == NORMAL_BOOT) || (get_boot_mode() == ALARM_BOOT)) {
		return g_black_data.status;
	}
	return BLACK_STATUS_INIT_SUCCEES;
#endif
}

static bool get_log_swich()
{
	return  (BLACK_STATUS_CHECK_ENABLE == get_status() || BLACK_STATUS_CHECK_DEBUG == get_status()) && g_black_data.get_log;
}

/*
logmap format:
logmap{key1:value1;key2:value2;key3:value3 ...}
*/
static void get_blackscreen_check_dcs_logmap(char *logmap)
{
	char stages[512] = {0};
	int stages_len;

	stages_len = get_pwkey_stages(stages);
	snprintf(logmap, 512, "logmap{logType:%s;error_id:%s;error_count:%u;systemserver_pid:%d;stages:%s;catchlog:%s}", PWKKEY_BLACK_SCREEN_DCS_LOGTYPE,
		g_black_data.error_id, g_black_data.error_count, get_systemserver_pid(), stages, get_log_swich() ? "true" : "false");
}

/* if the error id contain current pid, we think is a normal resume */
static bool is_normal_resume()
{
	char current_pid_str[32];
	sprintf(current_pid_str, "%d", get_systemserver_pid());
	if (!strncmp(g_black_data.error_id, current_pid_str, strlen(current_pid_str))) {
		return true;
	}

	return false;
}

static void get_blackscreen_resume_dcs_logmap(char *logmap)
{
	snprintf(logmap, 512, "logmap{logType:%s;error_id:%s;resume_count:%u;normalReborn:%s;catchlog:false}", PWKKEY_BLACK_SCREEN_DCS_LOGTYPE,
		g_black_data.error_id, g_black_data.error_count, (is_normal_resume() ? "true" : "false"));
}

void send_black_screen_dcs_msg(void)
{
	char logmap[512] = {0};
	get_blackscreen_check_dcs_logmap(logmap);
	SendDcsTheiaKevent(PWKKEY_DCS_TAG, PWKKEY_DCS_EVENTID, logmap);
}

static void send_black_screen_resume_dcs_msg(void)
{
	/* check the current systemserver pid and the error_id, judge if it is a normal resume or reboot resume */
	char resume_logmap[512] = {0};
	get_blackscreen_resume_dcs_logmap(resume_logmap);
	SendDcsTheiaKevent(PWKKEY_DCS_TAG, PWKKEY_DCS_EVENTID, resume_logmap);
}

static void delete_timer(char *reason, bool cancel)
{
	del_timer(&g_black_data.timer);

	if (cancel && g_black_data.error_count != 0) {
		send_black_screen_resume_dcs_msg();
		g_black_data.error_count = 0;
		sprintf(g_black_data.error_id, "%s", "null");
	}

	theia_pwk_stage_end(reason);
}

static ssize_t black_screen_cancel_proc_write(struct file *file, const char __user *buf,
	size_t count, loff_t *off)
{
	char buffer[40] = {0};
	char cancel_str[64] = {0};

	if (g_black_data.status == BLACK_STATUS_INIT || g_black_data.status == BLACK_STATUS_INIT_FAIL) {
		BLACK_DEBUG_PRINTK("%s init not finish: status = %d\n", __func__, g_black_data.status);
		return count;
	}

	if (count >= 40) {
		count = 39;
	}

	if (copy_from_user(buffer, buf, count)) {
		BLACK_DEBUG_PRINTK("%s: read proc input error.\n", __func__);
		return count;
	}

	snprintf(cancel_str, sizeof(cancel_str), "CANCELED_BL_%s", buffer);
    delete_timer(cancel_str, true);

	return count;
}

static ssize_t black_screen_cancel_proc_read(struct file *file, char __user *buf,
	size_t count, loff_t *off)
{
	return 0;
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static int black_screen_cancel_proc_show(struct seq_file *seq_file, void *data) {
	seq_printf(seq_file, "%s called\n", __func__);
	return 0;
}

static int black_screen_cancel_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, black_screen_cancel_proc_show, NULL);
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
static const struct proc_ops black_screen_cancel_proc_fops = {
	.proc_open = black_screen_cancel_proc_open,
	.proc_read = black_screen_cancel_proc_read,
	.proc_write = black_screen_cancel_proc_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#else
struct file_operations black_screen_cancel_proc_fops = {
	.read = black_screen_cancel_proc_read,
	.write = black_screen_cancel_proc_write,
};
#endif

static int black_write_error_count(struct black_data *bla_data)
{
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
	return 0;
#else
	struct file *fp;
	loff_t pos;
	ssize_t len = 0;
	char buf[256] = {'\0'};

	fp = filp_open(BLACKSCREEN_COUNT_FILE, O_RDWR | O_CREAT, 0664);
	if (IS_ERR(fp)) {
		BLACK_DEBUG_PRINTK("create %s file error fp:%p %d \n", BLACKSCREEN_COUNT_FILE, fp, PTR_ERR(fp));
		return -1;
	}

	sprintf(buf, "%d\n", bla_data->error_count);

	pos = 0;
	len = kernel_write(fp, buf, strlen(buf), &pos);
	if (len < 0)
		BLACK_DEBUG_PRINTK("write %s file error\n", BLACKSCREEN_COUNT_FILE);

	pos = 0;
	kernel_read(fp, buf, sizeof(buf), &pos);
	BLACK_DEBUG_PRINTK("black_write_error_count %s\n", buf);

	filp_close(fp, NULL);

	return len;
#endif
}

static void dump_freeze_log(void)
{
	/* send kevent dcs msg */
	send_black_screen_dcs_msg();
}

static void black_error_happen_work(struct work_struct *work)
{
	struct black_data *bla_data = container_of(work, struct black_data, error_happen_work);

	if (bla_data->error_count == 0) {
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0))
		struct timespec64 ts = { 0 };
		ktime_get_real_ts64(&ts);
		sprintf(g_black_data.error_id, "%d.%lld.%ld", get_systemserver_pid(), ts.tv_sec, ts.tv_nsec);
#else
		struct timespec ts;
		getnstimeofday(&ts);
		sprintf(g_black_data.error_id, "%d.%ld.%ld", get_systemserver_pid(), ts.tv_sec, ts.tv_nsec);
#endif
	}

	if (bla_data->error_count < BLACK_MAX_WRITE_NUMBER) {
		bla_data->error_count++;
		dump_freeze_log();
		black_write_error_count(bla_data);
	}
	BLACK_DEBUG_PRINTK("black_error_happen_work error_id = %s, error_count = %d\n",
		bla_data->error_id, bla_data->error_count);

	delete_timer("BR_ERROR_HAPPEN", false);

	if(bla_data->is_panic) {
		doPanic();
	}
}

static void black_timer_func(struct timer_list *t)
{
	struct black_data *p = from_timer(p, t, timer);
	BLACK_DEBUG_PRINTK("black_timer_func is called\n");

#if IS_ENABLED(CONFIG_DRM_PANEL_NOTIFY)
	if (g_black_data.active_panel == NULL || g_black_data.cookie == NULL) {
		BLACK_DEBUG_PRINTK("bl check register panel not ready\n");
		return;
	}
#endif

	if (bl_start_check_systemid == get_systemserver_pid()) {
		schedule_work(&p->error_happen_work);
	} else {
		BLACK_DEBUG_PRINTK("black_timer_func, not valid for check, skip\n");
	}
}

static int check_black_screen_tp_event(int *blank, unsigned long event)
{
	if (!event) {
		BLACK_DEBUG_PRINTK("black_fb_notifier_callback: Invalid event");
		return -1;
	}

	switch (event) {
	case THEIA_TOUCHPANEL_BLANK_EVENT:
	case THEIA_TOUCHPANEL_UNBLANK_EVENT:
	case THEIA_TOUCHPANEL_EARLY_BLANK_EVENT:
		g_black_data.blank = *blank;
		if (g_black_data.status != BLACK_STATUS_CHECK_DEBUG) {
			delete_timer("FINISH_FB", true);
			BLACK_DEBUG_PRINTK("black_fb_notifier_callback: del timer,event:%lu status:%d blank:%d\n",
				event, g_black_data.status, g_black_data.blank);
		} else {
			BLACK_DEBUG_PRINTK("black_fb_notifier_callback:event = %lu status:%d blank:%d\n",
				event, g_black_data.status, g_black_data.blank);
		}
		break;
	default:
		BLACK_DEBUG_PRINTK("black_fb_notifier_callback:event = %lu status:%d blank:%d\n",
			event, g_black_data.status, g_black_data.blank);
		break;
	}
return 0;
}

#if IS_ENABLED (CONFIG_DRM_PANEL_NOTIFY)
static void black_fb_notifier_callback(enum panel_event_notifier_tag tag,
	struct panel_event_notification *notification, void *client_data)
{
	int blank_flag = 0;
	if (notification && notification -> notif_type) {
		blank_flag = notification -> notif_type;
		check_black_screen_tp_event(&blank_flag, (unsigned long)(notification -> notif_type));
	}
}
#elif IS_ENABLED (CONFIG_DRM_MSM)
static int black_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	int ret = 0;
	struct msm_drm_notifier *evdata = data;
	int *blank;

	if (evdata && evdata->data) {
		blank = evdata->data;
	}
	ret = check_black_screen_tp_event(*blank, event);
	return ret;
}
#else
static int black_fb_notifier_callback(struct notifier_block *self,
	unsigned long event, void *data)
{
	int ret = 0;
	struct msm_drm_notifier *evdata = data;
	int *blank;

	if (evdata && evdata->data) {
		blank = evdata->data;
	}
	ret = check_black_screen_tp_event(*blank, event);
	return ret;
}
#endif

#if IS_ENABLED(CONFIG_DRM_PANEL_NOTIFY)
static int bl_register_panel_event_notify(void)
{
	void *data = NULL;
	void *cookie = NULL;

	cookie = panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,
		PANEL_EVENT_NOTIFIER_CLIENT_PRIMARY_THEIA_BLACK,
		g_black_data.active_panel, black_fb_notifier_callback, data);

	if (!cookie) {
		BLACK_DEBUG_PRINTK("bl_register_panel_event_notify failed\n");
		return -1;
	}
	g_black_data.cookie = cookie;
	return 0;
}

static struct drm_panel *theia_check_panel_dt_compatible(void)
{
	int i;
	int count;
	struct device_node *node = NULL;
	struct drm_panel *panel = NULL;
	struct device_node *np = NULL;

	np = of_find_node_by_name(NULL, "synaptics20031");
	if (!np) {
		BLACK_DEBUG_PRINTK("compatible Device tree info missing.\n");
		return NULL;
	} else {
		BLACK_DEBUG_PRINTK("compatible Device tree info found.\n");
	}

	count = of_count_phandle_with_args(np, "panel", NULL);
	BLACK_DEBUG_PRINTK("compatible Device tree panel count = %d.\n", count);
	if (count <= 0)
		return NULL;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			BLACK_DEBUG_PRINTK("compatible Found active panel.\n");
			break;
		}
	}

	if (IS_ERR(panel))
		panel = NULL;

	return panel;
}

static struct drm_panel *theia_check_panel_dt(void)
{
	int i;
	int count;
	struct device_node *node = NULL;
	struct drm_panel *panel = NULL;
	struct device_node *np = NULL;

	np = of_find_node_by_name(NULL, "oplus,dsi-display-dev");
	if (!np) {
		BLACK_DEBUG_PRINTK("Device tree info missing.\n");
		goto TRY_COMPATIBLE;
	} else {
		BLACK_DEBUG_PRINTK("Device tree info found.\n");
	}

	count = of_count_phandle_with_args(np, "oplus,dsi-panel-primary", NULL);
	BLACK_DEBUG_PRINTK("Device tree oplus,dsi-panel-primary count = %d.\n", count);
	if (count <= 0)
		goto TRY_COMPATIBLE;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "oplus,dsi-panel-primary", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			BLACK_DEBUG_PRINTK("Found active panel.\n");
			break;
		}
	}

	if (IS_ERR(panel))
		panel = NULL;

	return panel;

TRY_COMPATIBLE:
	return theia_check_panel_dt_compatible();
}

static struct drm_panel *theia_get_active_panel(void)
{
	struct drm_panel *panel = NULL;

	if (g_active_panel)
		return g_active_panel;

	panel = theia_check_panel_dt();
	if (panel)
		g_active_panel = panel;

	return panel;
}

static int register_panel_event(void)
{
	int ret = -1;
	struct drm_panel *panel = NULL;

	panel = theia_get_active_panel();
	if (panel) {
		g_black_data.active_panel = panel;
		g_bright_data.active_panel = panel;
		ret = bl_register_panel_event_notify();
		ret |= br_register_panel_event_notify();
	} else {
		BLACK_DEBUG_PRINTK("theia_check_panel_dt failed, get no active panel\n");
	}
	return ret;
}

static void check_dt_work_func(struct work_struct *work)
{
	if (register_panel_event()) {
		BLACK_DEBUG_PRINTK("register_panel_event failed, retry, retry_count = %d\n", g_check_dt_retry_count);
		if (g_check_dt_retry_count) {
			schedule_delayed_work(&g_check_dt_work, msecs_to_jiffies(CHECK_DT_DELAY_MS));
			g_check_dt_retry_count--;
		} else {
			g_black_data.status = BLACK_STATUS_INIT_FAIL;
			g_bright_data.status = BRIGHT_STATUS_INIT_FAIL;
			BLACK_DEBUG_PRINTK("register_panel_event failed, the pwrkey monitor function disabled\n");
		}
	}
}
#endif

void black_screen_check_init(void)
{
	/*Register touchpanel notifier callback functions*/
#if IS_ENABLED (CONFIG_DRM_PANEL_NOTIFY)
	g_check_dt_retry_count = 2;
	INIT_DELAYED_WORK(&g_check_dt_work, check_dt_work_func);
	schedule_delayed_work(&g_check_dt_work, msecs_to_jiffies(CHECK_DT_DELAY_MS));
#elif IS_ENABLED (CONFIG_DRM_MSM)
	g_black_data.fb_notif.notifier_call = black_fb_notifier_callback;
	msm_drm_register_client(&g_black_data.fb_notif);
#else
	g_black_data.fb_notif.notifier_call = black_fb_notifier_callback;
	fb_register_client(&g_black_data.fb_notif);
#endif

	/* the node for cancel black screen check */
	proc_create("blackSwitch", S_IRWXUGO, NULL, &black_screen_cancel_proc_fops);

	INIT_WORK(&g_black_data.error_happen_work, black_error_happen_work);
	timer_setup((&g_black_data.timer), (black_timer_func), TIMER_DEFERRABLE);
	g_black_data.status = BLACK_STATUS_CHECK_ENABLE;
}

void black_screen_exit(void)
{
	delete_timer("FINISH_DRIVER_EXIT", true);
#if IS_ENABLED(CONFIG_DRM_PANEL_NOTIFY)
	if (g_black_data.active_panel && g_black_data.cookie)
		panel_event_notifier_unregister(g_black_data.cookie);
#elif IS_ENABLED(CONFIG_DRM_MSM)
	msm_drm_unregister_client(&g_black_data.fb_notif);
#else
	fb_unregister_client(&g_black_data.fb_notif);
#endif
}
MODULE_LICENSE("GPL v2");

