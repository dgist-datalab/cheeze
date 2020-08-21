// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <linux/module.h>
#include <linux/delay.h>

#include "cheeze.h"

static unsigned long front, rear, qsize;

// Protect with lock
struct cheeze_req *reqs = NULL;
static DEFINE_MUTEX(mutex);

static inline bool empty(void) {
	return (front == rear);
}

// Lock must be held and freed before and after push()
unsigned long cheeze_push(const int rw,
		 const unsigned int index,
		 const unsigned int offset,
		 const unsigned int size,
		 void *addr) {
	struct cheeze_req *req;

	while (unlikely((rear + 1) % CHEEZE_QUEUE_SIZE == front)) {
		// Full
		mb();
		pr_err("%s: queue is full\n", __func__);
		//msleep(100);
	}

	mutex_lock(&mutex);

	qsize++;
	rear = (rear + 1) % CHEEZE_QUEUE_SIZE;

	req = reqs + rear;
	req->acked = 0;
	req->rw = rw;
	req->index = index;
	req->offset = offset;
	req->size = size;
	req->addr = addr;
	req->id = rear;

	pr_debug("req[%lu]\n"
		"  rw=%d\n"
		"  index=%u\n"
		"  offset=%u\n"
		"  size=%u\n"
		"  addr=%p\n",
			rear, rw, index, offset, size, addr);

	mutex_unlock(&mutex);

	return rear;
}

struct cheeze_req *cheeze_pop(void) {
	while (unlikely(empty())) {
		mb();
		// Nothing to pop
		//pr_err("%s: queue is empty\n", __func__);
		//mdelay(100);
		//usleep_range(50, 75);
	}

	mutex_lock(&mutex);

	qsize--;
	front = (front + 1) % CHEEZE_QUEUE_SIZE;

	mutex_unlock(&mutex);

	return reqs + front;
}
