// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */


#include <linux/crc32.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include "cheeze.h"

static void *page_addr[3];
//static void *meta_addr; // page_addr[0] ==> send_event_addr, recv_event_addr, seq_addr, ureq_addr
static uint64_t *send_event_addr; // CHEEZE_QUEUE_SIZE ==> 16B
static uint64_t *recv_event_addr; // 16B
static uint64_t *seq_addr; // 8KB
struct cheeze_req_user *ureq_addr; // sizeof(req) * 1024
static char *data_addr[2]; // page_addr[1]: 1GB, page_addr[2]: 1GB

static struct task_struct *shm_task = NULL;

int send_req (struct cheeze_req *req, int id, uint64_t seq) {
	int idx = id / CHEEZE_QUEUE_SIZE;
	uint64_t *send = &send_event_addr[ (id / BITS_PER_EVENT) ];
	char *buf = get_buf_addr(data_addr, id);
	struct cheeze_req_user *ureq = ureq_addr + id;
	// caller should be call memcpy to reqs before calling this function
	memcpy(buf, req->user->buf, CHEEZE_BUF_SIZE);
	memcpy(ureq, req->user, sizeof(*ureq));
	seq_addr[id] = seq;
	/* memory barrier XXX:Arm */
	*send = *send | (1ULL << (id % BITS_PER_EVENT));
	/* memory barrier XXX:Arm */
	return 0;
}

static void recv_req (void) {
	uint64_t *recv;
	int i, id, j;
	uint64_t mask;
	struct cheeze_req *req;
	struct cheeze_req_user *ureq;

	for (i = 0; i < CHEEZE_QUEUE_SIZE / BITS_PER_EVENT; i++) {
		recv = &recv_event_addr[i];
		for (j = 0; j < BITS_PER_EVENT; j++) {
			mask = 1ULL << j;
			if (*recv & mask) {
				id = i * BITS_PER_EVENT + j;
				req = reqs + id;
				ureq = ureq_addr + id;
				if (ureq->ret_buf == NULL) { // SET
					complete(&req->acked);
				} else {
					if (ureq->ubuf_len != 0) { // GET
						memcpy(ureq->ret_buf, ureq->buf, ureq->ubuf_len);
					}
					complete(&req->acked);
				}
				/* memory barrier XXX:Arm */
				*recv = *recv & ~mask;
				/* memory barrier XXX:Arm */
			}
		}
	}
}

static int shm_kthread(void *unused)
{
	while (!kthread_should_stop()) {
		recv_req();
		cond_resched();
	}

	return 0;
}

static int set_page_addr0(const char *val, const struct kernel_param *kp)
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
	page_addr[0] = phys_to_virt(dst);
	pr_info("page_addr: 0x%px\n", page_addr[0]);

	return ret;
}

const struct kernel_param_ops page_addr_ops0 = {
	.set = set_page_addr0,
	.get = NULL
};

module_param_cb(page_addr0, &page_addr_ops0, NULL, 0644);



static int set_page_addr1(const char *val, const struct kernel_param *kp)
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
	page_addr[1] = phys_to_virt(dst);
	pr_info("page_addr: 0x%px\n", page_addr[1]);

	return ret;
}

const struct kernel_param_ops page_addr_ops1 = {
	.set = set_page_addr1,
	.get = NULL
};

module_param_cb(page_addr1, &page_addr_ops1, NULL, 0644);



static int set_page_addr2(const char *val, const struct kernel_param *kp)
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
	page_addr[2] = phys_to_virt(dst);
	pr_info("page_addr: 0x%px\n", page_addr[2]);

	return ret;
}

const struct kernel_param_ops page_addr_ops2 = {
	.set = set_page_addr2,
	.get = NULL
};

module_param_cb(page_addr2, &page_addr_ops2, NULL, 0644);



static bool enable;
static int enable_param_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	if (page_addr == NULL)
		return -EINVAL;

	ret = param_set_bool(val, kp);

	if (enable) {
		pr_info("Enabling shm\n");
		shm_task = kthread_run(shm_kthread, NULL, "kshm");
		pr_info("Enabled shm\n");
	} else {
		pr_info("Disabling shm\n");
		kthread_stop(shm_task);
		shm_task = NULL;
		pr_info("Disabled shm\n");
	}

	return ret;
}

static struct kernel_param_ops enable_param_ops = {
	.set = enable_param_set,
	.get = param_get_bool,
};

module_param_cb(enabled, &enable_param_ops, &enable, 0644);

static void shm_meta_init(void *ppage_addr) {
	memset(ppage_addr, 0, (1ULL * 1024 * 1024 * 1024));
	send_event_addr = ppage_addr + SEND_OFF; // CHEEZE_QUEUE_SIZE ==> 16B
	recv_event_addr = ppage_addr + RECV_OFF; // 16B
	seq_addr = ppage_addr + SEQ_OFF; // 8KB
	ureq_addr = ppage_addr + REQS_OFF; // sizeof(req) * 1024
}

static void shm_data_init(void **ppage_addr) {
	data_addr[0] = ppage_addr[1];
	data_addr[1] = ppage_addr[2];
}

static void __exit shm_exit(void)
{
	if (!shm_task)
		return;

	kthread_stop(shm_task);
	shm_task = NULL;
}

//module_exit(shm_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Park Ju Hyung <qkrwngud825@gmail.com>");
