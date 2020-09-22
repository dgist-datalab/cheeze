// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#define pr_fmt(fmt) "pingpong: " fmt

// #define DEBUG

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>

static void *page_addr;

static struct task_struct *pingpong_task = NULL;

static int pingpong_kthread(void *unused)
{
	while (!kthread_should_stop()) {
	}

	return 0;
}

static int set_page_addr(const char *val, const struct kernel_param *kp)
{
	unsigned long dst;
	int ret;

	if (strncmp(val, "0x", 2))
		ret = kstrtoul(val, 16, &dst);
	else
		ret = kstrtoul(val + 2, 16, &dst);

	if (ret < 0)
		return ret;

	pr_info("Setting 0x%lx as page address\n", dst);
	memcpy(&page_addr, &dst, sizeof(void*));
	pr_info("page_addr: %px\n", page_addr);

	return ret;
}

const struct kernel_param_ops page_addr_ops = {
	.set = set_page_addr,
	.get = NULL
}; 

module_param_cb(page_addr, &page_addr_ops, NULL, 0644);

static bool enable;
static int enable_param_set(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_bool(val, kp);

	if (enable) {
		pr_info("Enabling pingpong\n");
		pingpong_task = kthread_run(pingpong_kthread, NULL, "kpingpong");
		pr_info("Enabled pingpong\n");
	} else {
		pr_info("Disabling pingpong\n");
		kthread_stop(pingpong_task);
		pingpong_task = NULL;
		pr_info("Disabled pingpong\n");
	}

	return ret;
}

static struct kernel_param_ops enable_param_ops = {
	.set = enable_param_set,
	.get = param_get_bool,
};

module_param_cb(enabled, &enable_param_ops, &enable, 0644);

static void __exit pingpong_exit(void)
{
	if (!pingpong_task)
		return;

	kthread_stop(pingpong_task);
	pingpong_task = NULL;
}

module_exit(pingpong_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Park Ju Hyung <qkrwngud825@gmail.com>");
