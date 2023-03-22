// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2022 Oplus. All rights reserved.
 */
#ifdef CONFIG_LOCKING_PROTECT
#define pr_fmt(fmt) "dstate_opt: " fmt

#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/rwsem.h>
#include <linux/ww_mutex.h>
#include <linux/sched/signal.h>
#include <linux/sched/rt.h>
#include <linux/sched/wake_q.h>
#include <linux/sched/debug.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/debug_locks.h>
#include <linux/osq_lock.h>
#include <linux/sched_clock.h>
#include <../kernel/sched/sched.h>
#include <trace/hooks/vendor_hooks.h>
#include <trace/hooks/sched.h>
#include <trace/hooks/dtask.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/rwsem.h>
#include <trace/hooks/futex.h>
#include <trace/hooks/fpsimd.h>
#include <trace/hooks/topology.h>
#include <trace/hooks/debug.h>
#include <trace/hooks/wqlockup.h>
#include <trace/hooks/cgroup.h>
#include <trace/hooks/sys.h>
#include <trace/hooks/mm.h>
#include <linux/jiffies.h>
#include <../../sched/sched_assist/sa_common.h>

bool locking_protect_disable = false;

static int locking_max_over_thresh = 2;
static int locking_entity_over(struct sched_entity *a, struct sched_entity *b)
{
	return (s64)(a->vruntime - b->vruntime) >
			(s64)locking_max_over_thresh * NSEC_PER_SEC;
}

unsigned int locking_wakeup_preepmt_enable;

bool task_skip_protect(struct task_struct *p)
{
	return test_task_ux(p);
}

bool task_inlock(struct oplus_task_struct *ots)
{
	if (locking_protect_disable ==  true) {
		locking_wakeup_preepmt_enable = 0;
		return false;
	}

	return ots->locking_start_time > 0;
}

bool locking_protect_outtime(struct oplus_task_struct *ots)
{
	return time_after(jiffies, ots->locking_start_time);
}

void clear_locking_info(struct oplus_task_struct *ots)
{
	ots->locking_start_time = 0;
}

void enqueue_locking_thread(struct rq *rq, struct task_struct *p)
{
	struct oplus_task_struct *ots = NULL;
	struct oplus_rq *orq = NULL;
	struct list_head *pos, *n;

	if (!rq || !p || locking_protect_disable)
		return;

	ots = get_oplus_task_struct(p);
	orq = (struct oplus_rq *) rq->android_oem_data1;

	if (!oplus_list_empty(&ots->locking_entry))
		return;

	if (task_inlock(ots)) {
		bool exist = false;

		list_for_each_safe(pos, n, &orq->locking_thread_list) {
			if (pos == &ots->locking_entry) {
				exist = true;
				break;
			}
		}
		if (!exist) {
			list_add_tail(&ots->locking_entry, &orq->locking_thread_list);
			orq->rq_locking_task++;
			get_task_struct(p);
		}
	}
}

void dequeue_locking_thread(struct rq *rq, struct task_struct *p)
{
	struct oplus_task_struct *ots = NULL;
	struct oplus_rq *orq = NULL;
	struct list_head *pos, *n;

	if (!rq || !p || locking_protect_disable)
		return;

	ots = get_oplus_task_struct(p);
	orq = (struct oplus_rq *) rq->android_oem_data1;

	if (!oplus_list_empty(&ots->locking_entry)) {
		list_for_each_safe(pos, n, &orq->locking_thread_list) {
			if (pos == &ots->locking_entry) {
				list_del_init(&ots->locking_entry);
				orq->rq_locking_task--;
				put_task_struct(p);
				return;
			}
		}
	}
}

void oplus_replace_locking_task_fair(struct rq *rq, struct task_struct **p,
					struct sched_entity **se, bool *repick)
{
	struct oplus_rq *orq = NULL;
	struct task_struct *ori_p = *p;

	if (!rq || !p || !se || locking_protect_disable)
		return;

	orq = (struct oplus_rq *)rq->android_oem_data1;

pick_again:
	if (!list_empty(&orq->locking_thread_list)) {
		struct sched_entity *key_se;
		struct task_struct *key_task;
		struct oplus_task_struct *key_ots =
			list_first_entry_or_null(&orq->locking_thread_list,
					struct oplus_task_struct, locking_entry);

		if (!key_ots)
			return;

		key_task = ots_to_ts(key_ots);
		if (key_task) {
			if (locking_entity_over(&key_task->se, &ori_p->se))
				return;

			if (task_inlock(key_ots)) {
				key_se = &key_task->se;
				if (key_se) {
					*p = key_task;
					*se = key_se;
					*repick = true;
					return;
				}
			} else {
				list_del_init(&key_ots->locking_entry);
				orq->rq_locking_task--;
				put_task_struct(key_task);
			}
			goto pick_again;
		}
	}
}

static void record_lock_starttime_handler(void *unused,
			struct task_struct *tsk, unsigned long settime)
{
	struct oplus_task_struct *ots = get_oplus_task_struct(tsk);

	if (settime) {
		ots->locking_depth++;
	} else {
		if (ots->locking_depth > 0) {
			if (--(ots->locking_depth))
				return;
		}
	}

	ots->locking_start_time = settime;
}

static void check_preempt_tick_handler(void *unused, struct task_struct *p,
			unsigned long *ideal_runtime, bool *skip_preempt,
			unsigned long delta_exec, struct cfs_rq *cfs_rq,
			struct sched_entity *curr, unsigned int granularity)
{
	struct task_struct *curr_tsk = container_of(curr, struct task_struct, se);

	if (curr_tsk && !curr->my_q) {
		struct oplus_task_struct *ots = get_oplus_task_struct(curr_tsk);

		if (task_inlock(ots)) {
			if (locking_protect_outtime(ots))
				clear_locking_info(ots);
		}
	}
}

static int register_dstate_opt_vendor_hooks(void)
{
	int ret = 0;

	ret = register_trace_android_vh_record_mutex_lock_starttime(
					record_lock_starttime_handler, NULL);
	if (ret != 0) {
		pr_err("android_vh_record_mutex_lock_starttime failed! ret=%d\n", ret);
		goto out;
	}

	ret = register_trace_android_vh_record_rtmutex_lock_starttime(
					record_lock_starttime_handler, NULL);
	if (ret != 0) {
		pr_err("android_vh_record_rtmutex_lock_starttime failed! ret=%d\n", ret);
		goto out1;
	}

	ret = register_trace_android_vh_record_rwsem_lock_starttime(
					record_lock_starttime_handler, NULL);
	if (ret != 0) {
		pr_err("record_rwsem_lock_starttime failed! ret=%d\n", ret);
		goto out2;
	}

	ret = register_trace_android_rvh_check_preempt_tick(check_preempt_tick_handler,
								NULL);
	if (ret != 0) {
		pr_err("register_trace_android_rvh_check_preempt_tick failed! ret=%d\n", ret);
		goto out3;
	}

	return ret;
out3:
	unregister_trace_android_vh_record_rwsem_lock_starttime(
				record_lock_starttime_handler, NULL);
out2:
	unregister_trace_android_vh_record_rtmutex_lock_starttime(
				record_lock_starttime_handler, NULL);
out1:
	unregister_trace_android_vh_record_mutex_lock_starttime(
				record_lock_starttime_handler, NULL);
out:
	return ret;
}

static void unregister_dstate_opt_vendor_hooks(void)
{
	unregister_trace_android_vh_record_mutex_lock_starttime(
				record_lock_starttime_handler, NULL);
	unregister_trace_android_vh_record_rtmutex_lock_starttime(
				record_lock_starttime_handler, NULL);
	unregister_trace_android_vh_record_rwsem_lock_starttime(
				record_lock_starttime_handler, NULL);
}

int sched_assist_locking_init(void)
{
	int ret = 0;

	ret = register_dstate_opt_vendor_hooks();
	if (ret != 0)
		return ret;

	pr_info("%s succeed!\n", __func__);
	return 0;
}

void sched_assist_locking_exit(void)
{
	unregister_dstate_opt_vendor_hooks();
	pr_info("%s exit init succeed!\n", __func__);
}
#endif
