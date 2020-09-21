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
#include <linux/spinlock.h>

#include "cheeze.h"

//static int front, rear;
//static struct semaphore mutex, slots, items;
static struct semaphore slots, items;
static struct list_head free_tag_list, processing_tag_list; 
static spinlock_t queue_spin;


// Protect with lock
struct cheeze_req *reqs = NULL;

// Lock must be held and freed before and after push()
int cheeze_push(struct cheeze_req_user *user) {
	struct cheeze_req *req;
	int id;
	unsigned long irqflags;
	struct cheeze_queue_item *item; 

	while(down_interruptible(&slots) == -EINTR) {
		//pr_info("interrupt - 1\n");
	}
	spin_lock_irqsave(&queue_spin, irqflags);
	//while(down_interruptible(&mutex) == -EINTR) {
	//	pr_info("interrupt - 2\n");
	//}
	//down(&mutex);

	item = list_first_entry(&free_tag_list, struct cheeze_queue_item, tag_list);
	list_move_tail(&item->tag_list, &processing_tag_list);
	id = item->id;

	//id = (rear + 1) % CHEEZE_QUEUE_SIZE; // XXX: Overflow?

	//pr_info("pushing %d(buf_len: %d)\n", id, user->buf_len);

	//rear = id;
	req = reqs + id;		/* Insert the item */

	req->user = user;
	req->user->id = id;
	reinit_completion(&req->acked);
	req->item = item;

	spin_unlock_irqrestore(&queue_spin, irqflags);

	//up(&mutex);	/* Unlock the buffer */
	up(&items);	/* Announce available item */

	return id;
}

// Queue is locked until pop
struct cheeze_req *cheeze_peek(void) {
	int id, ret;
	struct cheeze_queue_item *item;
	unsigned long irqflags;

	ret = down_interruptible(&items);	/* Wait for available item */
	if (unlikely(ret < 0))
		return NULL;

	/* Lock the buffer */
	//down(&mutex);
	spin_lock_irqsave(&queue_spin, irqflags);
	//while(down_interruptible(&mutex) == -EINTR) {
	//	pr_info("interrupt - 3\n");
	//}
	item = list_first_entry(&processing_tag_list, struct cheeze_queue_item, tag_list);
	list_del(&item->tag_list);
	id = item->id;

	//id = (front + 1) % CHEEZE_QUEUE_SIZE;	/* Remove the item */
	spin_unlock_irqrestore(&queue_spin, irqflags);

	return reqs + id;
}

void cheeze_pop(int id) {
	unsigned long irqflags;
	struct cheeze_queue_item *item;
	struct cheeze_req *req;

	spin_lock_irqsave(&queue_spin, irqflags);

	req = reqs + id;
	
	item = req->item;
	//list_move_tail(&item->tag_list, &free_tag_list);
	list_add_tail(&item->tag_list, &free_tag_list);

	spin_unlock_irqrestore(&queue_spin, irqflags);

	//up(&mutex);	/* Unlock the buffer */
	up(&slots);	/* Announce available slot */
}



void cheeze_queue_init(void) {
	int i;
	struct cheeze_queue_item *item;
	//front = rear = 0;	/* Empty buffer iff front == rear */
	//sema_init(&mutex, 1);	/* Binary semaphore for locking */
	INIT_LIST_HEAD(&free_tag_list);
	INIT_LIST_HEAD(&processing_tag_list);
	spin_lock_init(&queue_spin);

	for (i = 0; i < CHEEZE_QUEUE_SIZE; i++) {
		item = kzalloc(sizeof(struct cheeze_queue_item), GFP_NOIO);
		item->id = i;
		INIT_LIST_HEAD(&item->tag_list);
		list_add_tail(&item->tag_list, &free_tag_list);
	}

	sema_init(&slots, CHEEZE_QUEUE_SIZE);	/* Initially, buf has n empty slots */
	sema_init(&items, 0);	/* Initially, buf has zero data items */
}


void cheeze_queue_exit(void) {
	int i;
	struct cheeze_req *req;
	for (i = 0; i < CHEEZE_QUEUE_SIZE; i++) {
		req = reqs + i;
		kfree(req->item);
	}
}
