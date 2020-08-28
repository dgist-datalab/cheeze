// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/semaphore.h>

#include "cheeze.h"

static int front, rear;
static struct semaphore mutex, slots, items;

// Protect with lock
struct cheeze_req *reqs = NULL;

// Lock must be held and freed before and after push()
int cheeze_push(const int rw,
		 const unsigned int offset,
		 const unsigned int size,
		 void *addr) {
	struct cheeze_req *req;
	int id, ret;

	ret = down_interruptible(&slots);	/* Wait for available slot */
	if (unlikely(ret < 0))
		return ret;

	ret = down_interruptible(&mutex);	/* Lock the buffer */
	if (unlikely(ret < 0))
		return ret;

	id = (rear + 1) % CHEEZE_QUEUE_SIZE; // XXX: Overflow?
	pr_info("pushing %d(%d)\n", id, reqs[id].id);
	rear = id;
	req = reqs + id;		/* Insert the item */

	reinit_completion(&req->acked);
	req->rw = rw;
	req->offset = offset;
	req->size = size;
	req->addr = addr;
	req->id = id;

	pr_debug("req[%d]\n"
		"  rw=%d\n"
		"  offset=%u\n"
		"  size=%u\n"
		"  addr=%p\n",
			rear, rw, offset, size, addr);

	up(&mutex);	/* Unlock the buffer */
	up(&items);	/* Announce available item */

	return id;
}

struct cheeze_req *cheeze_pop(void) {
	int id, ret;

	ret = down_interruptible(&items);	/* Wait for available item */
	if (unlikely(ret < 0))
		return NULL;

	ret = down_interruptible(&mutex);	/* Lock the buffer */
	if (unlikely(ret < 0))
		return NULL;

	id = (front + 1) % CHEEZE_QUEUE_SIZE;	/* Remove the item */
	pr_info("popping %d(%d)\n", id, reqs[id].id);
	front = id;

	up(&mutex);	/* Unlock the buffer */
	up(&slots);	/* Announce available slot */

	return reqs + id;
}

void cheeze_queue_init(void) {
	front = rear = 0;	/* Empty buffer iff front == rear */
	sema_init(&mutex, 1);	/* Binary semaphore for locking */
	sema_init(&slots, CHEEZE_QUEUE_SIZE);	/* Initially, buf has n empty slots */
	sema_init(&items, 0);	/* Initially, buf has zero data items */
}
