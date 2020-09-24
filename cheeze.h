// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#ifndef __CHEEZE_H
#define __CHEEZE_H

#define CHEEZE_QUEUE_SIZE 1024
#define BUF_SIZE (2ULL * 1024 * 1024)
#define ITEMS_PER_HP ((1ULL * 1024 * 1024 * 1024) / BUFSIZE)
#define BITS_PER_EVENT (sizeof(uint64_t) * 8)

#define EVENT_BYTES (CHEEZE_QUEUE_SIZE / BITS_PER_EVNENT)

#define SEND_OFF 0
#define SEND_SIZE EVENT_BYTES

#define RECV_OFF (SEND_OFF + SEND_SIZE)
#define RECV_SIZE EVENT_BYTES

#define SEQ_OFF (RECV_OFF + RECV_SIZE)
#define SEQ_SIZE (sizeof(uint64_t) * CHEEZE_QUEUE_SIZE)

#define REQS_OFF (SEQ_OFF + SEQ_SIZE)
#define REQS_SIZE (sizeof(struct cheeze_req) * CHEEZE_QUEUE_SIZE)

#define SKIP INT_MIN

// #define DEBUG
#define DEBUG_SLEEP 1

#ifdef DEBUG
  #define msleep_dbg msleep
#else
  #define msleep_dbg(...) ((void)0)
#endif

struct cheeze_req_user {
	int id; // Set by cheeze
	int buf_len; // Set by koo
	char *buf; // Set by koo
	int ubuf_len; // Set by bom
	char __user *ubuf; // Set by bom
	char *ret_buf; // Set by koo (Could be NULL)
} __attribute__((aligned(8), packed));

#ifdef __KERNEL__

#include <linux/list.h>

struct cheeze_queue_item {
	int id;
	struct list_head tag_list;
};

struct cheeze_req {
	struct completion acked; // Set by cheeze
	struct cheeze_req_user *user; // Set by koo, needs to be freed by koo
	struct cheeze_queue_item *item;
};

// blk.c
void cheeze_io(struct cheeze_req_user *user, void *(*cb)(void *data), void *extra); // Called by koo
extern struct class *cheeze_chr_class;
// extern struct mutex cheeze_mutex;
void cheeze_chr_cleanup_module(void);
int cheeze_chr_init_module(void);

// queue.c
extern struct cheeze_req *reqs;
uint64_t cheeze_push(struct cheeze_req_user *user);
struct cheeze_req *cheeze_peek(void);
void cheeze_pop(int id);
void cheeze_move_pop(int id);
void cheeze_queue_init(void);
void cheeze_queue_exit(void);

//shm.c
int send_req (struct cheeze_req *req, int id, uint64_t seq);
static inline get_buf_addr(int id) {
	int idx = id / ITEMS_PER_HP;
	return data_addr[idx] + (id * BUF_SIZE);
}

#endif

#endif
