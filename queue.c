// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <linux/module.h>
#include <linux/delay.h>

#include "cheeze.h"

static int front, rear;
static int qsize;

// Protect with lock
struct cheeze_req *reqs = NULL;
static DEFINE_MUTEX(mutex);

static inline bool empty(void) {
	return (front == rear);
}

// Lock must be held and freed before and after push()
void cheeze_push(const int rw,
		 const unsigned int index,
		 const unsigned int offset,
		 const unsigned int size,
		 const unsigned int addr) {
	struct cheeze_req *req;

	while (unlikely((rear + 1) % CHEEZE_QUEUE_SIZE == front)) {
		// Full
		pr_err("%s: queue is full\n", __func__);
		mdelay(100);
	}

	mutex_lock(&mutex);

	qsize++;
	rear = (rear + 1) % CHEEZE_QUEUE_SIZE;

	req = reqs + rear;
	req->rw = rw;
	req->index = index;
	req->offset = offset;
	req->size = size;
	req->addr = addr;

	mutex_unlock(&mutex);
}

struct cheeze_req *cheeze_pop(void) {
	while (unlikely(empty())) {
		// Nothing to pop
		pr_err("%s: queue is empty\n", __func__);
		mdelay(100);
	}

	mutex_lock(&mutex);

	qsize--;
	front = (front + 1) % CHEEZE_QUEUE_SIZE;

	mutex_unlock(&mutex);

	return reqs + front;
}
