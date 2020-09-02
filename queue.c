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
int cheeze_push(struct request *rq) {
	struct cheeze_req *req;
	int id, op;
	bool is_rw = true;

	op = req_op(rq);
	if (unlikely(op > 1)) {
		is_rw = false;
		switch (op = req_op(rq)) {
		case REQ_OP_FLUSH:
			pr_warn("ignoring REQ_OP_FLUSH\n");
			return SKIP;
		case REQ_OP_WRITE_ZEROES:
			// pr_warn("ignoring REQ_OP_WRITE_ZEROES\n");
			// return SKIP;
			pr_debug("REQ_OP_WRITE_ZEROES -> REQ_OP_DISCARD\n");
			op = REQ_OP_DISCARD;
			/* fallthrough */
		case REQ_OP_DISCARD:
			pr_debug("REQ_OP_DISCARD\n");
			break;
		default:
			pr_warn("unsupported operation: %d\n", op);
			return -EOPNOTSUPP;
		}
	}

	down(&slots); /* Wait for available slot */
	down(&mutex); /* Lock the buffer */

	id = (rear + 1) % CHEEZE_QUEUE_SIZE; // XXX: Overflow?
	// pr_info("pushing %d(%d)\n", id, reqs[id].id);
	rear = id;
	req = reqs + id;		/* Insert the item */

	req->rq = rq;
	req->is_rw = is_rw;

	req->user.op = op;
	req->user.pos = (blk_rq_pos(rq) << SECTOR_SHIFT) >> CHEEZE_LOGICAL_BLOCK_SHIFT;
	req->user.len = blk_rq_bytes(rq);
	req->user.id = id;
	reinit_completion(&req->acked);

	pr_debug("req[%d]\n"
		"  pos=%u\n"
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

	/* Lock the buffer */
	down(&mutex);

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
