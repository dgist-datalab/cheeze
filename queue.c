// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <linux/module.h>
#include <linux/delay.h>

#include "cheeze.h"

static int front, rear, qsize;

// Protect with lock
struct cheeze_req *reqs = NULL;
static DEFINE_MUTEX(mutex);
static DECLARE_COMPLETION(queue_added);
static DECLARE_COMPLETION(queue_freed);

static inline bool empty(void) {
	return (front == rear);
}

// Lock must be held and freed before and after push()
int cheeze_push(const int rw,
		 const unsigned int index,
		 const unsigned int offset,
		 const unsigned int size,
		 void *addr) {
	struct cheeze_req *req;
	int ret;

	while (unlikely((rear + 1) % CHEEZE_QUEUE_SIZE == front)) {
		ret = wait_for_completion_interruptible(&queue_freed);
		if (ret)
			return ret;
	}

	mutex_lock(&mutex);

	qsize++;
	rear = (rear + 1) % CHEEZE_QUEUE_SIZE;

	req = reqs + rear;
	reinit_completion(&req->acked);
	req->rw = rw;
	req->index = index;
	req->offset = offset;
	req->size = size;
	req->addr = addr;
	req->id = rear;

	pr_debug("req[%d]\n"
		"  rw=%d\n"
		"  index=%u\n"
		"  offset=%u\n"
		"  size=%u\n"
		"  addr=%p\n",
			rear, rw, index, offset, size, addr);

	mutex_unlock(&mutex);

	complete(&queue_added);

	return rear;
}

struct cheeze_req *cheeze_pop(void) {
	int ret;

	while (unlikely(empty())) {
		ret = wait_for_completion_interruptible(&queue_added);
		if (ret)
			return NULL;
	}

	mutex_lock(&mutex);

	qsize--;
	front = (front + 1) % CHEEZE_QUEUE_SIZE;

	mutex_unlock(&mutex);

	complete(&queue_freed);

	return reqs + front;
}
