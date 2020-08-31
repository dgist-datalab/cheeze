// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/semaphore.h>
#include <linux/blkdev.h>
#include <linux/genhd.h>
#include <linux/backing-dev.h>
#include <linux/blk-mq.h>

#include "cheeze.h"

static int front, rear;
static struct semaphore mutex, slots, items;

// Protect with lock
struct cheeze_req *reqs = NULL;

// Lock must be held and freed before and after push()
int cheeze_push(struct request *rq, unsigned int *nr_bytes) {
	struct cheeze_req *req;
	int id, ret;

	ret = down_interruptible(&slots);	/* Wait for available slot */
	if (unlikely(ret < 0))
		return ret;

	ret = down_interruptible(&mutex);	/* Lock the buffer */
	if (unlikely(ret < 0))
		return ret;

	id = (rear + 1) % CHEEZE_QUEUE_SIZE; // XXX: Overflow?
	// pr_info("pushing %d(%d)\n", id, reqs[id].id);
	rear = id;
	req = reqs + id;		/* Insert the item */

	req->rq = rq;
	req->nr_bytes = nr_bytes;
	req->user.pos = blk_rq_pos(rq) << SECTOR_SHIFT;
	req->user.len = blk_rq_bytes(rq);
	req->user.id = id;
	reinit_completion(&req->acked);

	pr_debug("req[%d]\n"
		"  pos=%lu\n"
		"  len=%u\n",
			rear, req->user.pos, req->user.len);

	up(&mutex);	/* Unlock the buffer */
	up(&items);	/* Announce available item */

	return id;
}

// Queue is locked until pop
struct cheeze_req *cheeze_peek(void) {
	int id, ret;

	ret = down_interruptible(&items);	/* Wait for available item */
	if (unlikely(ret < 0))
		return NULL;

	ret = down_interruptible(&mutex);	/* Lock the buffer */
	if (unlikely(ret < 0))
		return NULL;

	id = (front + 1) % CHEEZE_QUEUE_SIZE;	/* Remove the item */

	return reqs + id;
}

void cheeze_pop(int id) {
	front = id;

	up(&mutex);	/* Unlock the buffer */
	up(&slots);	/* Announce available slot */
}

void cheeze_queue_init(void) {
	front = rear = 0;	/* Empty buffer iff front == rear */
	sema_init(&mutex, 1);	/* Binary semaphore for locking */
	sema_init(&slots, CHEEZE_QUEUE_SIZE);	/* Initially, buf has n empty slots */
	sema_init(&items, 0);	/* Initially, buf has zero data items */
}
