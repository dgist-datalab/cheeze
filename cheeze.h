// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#ifndef __CHEEZE_H
#define __CHEEZE_H

#define SECTOR_SHIFT		9
#define SECTOR_SIZE		(1 << SECTOR_SHIFT)
#define SECTORS_PER_PAGE_SHIFT	(PAGE_SHIFT - SECTOR_SHIFT)
#define SECTORS_PER_PAGE	(1 << SECTORS_PER_PAGE_SHIFT)
#define CHEEZE_LOGICAL_BLOCK_SHIFT 12
#define CHEEZE_LOGICAL_BLOCK_SIZE	(1 << CHEEZE_LOGICAL_BLOCK_SHIFT)
#define CHEEZE_SECTOR_PER_LOGICAL_BLOCK	(1 << \
	(CHEEZE_LOGICAL_BLOCK_SHIFT - SECTOR_SHIFT))

#define CHEEZE_QUEUE_SIZE 1024
#define CHEEZE_BUF_SIZE (2ULL * 1024 * 1024)
#define HP_SIZE (1024L * 1024L * 1024L)
#define ITEMS_PER_HP (HP_SIZE / CHEEZE_BUF_SIZE)
#define BITS_PER_EVENT (sizeof(uint64_t) * 8)

#define EVENT_BYTES (CHEEZE_QUEUE_SIZE / BITS_PER_EVENT)

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
	int id;
	int op;
	unsigned int pos; // sector_t but divided by 4096
	unsigned int len;
} __attribute__((aligned(8), packed));

#ifdef __KERNEL__

#include <linux/list.h>

struct cheeze_queue_item {
	int id;
	struct list_head tag_list;
};

struct cheeze_req {
	int ret;
	bool is_rw;
	struct request *rq;
	struct cheeze_req_user user;
	struct completion acked;
	struct cheeze_queue_item *item;
} __attribute__((aligned(8), packed));

// blk.c
void cheeze_io(struct cheeze_req_user *user); // Called by koo
extern struct class *cheeze_chr_class;
// extern struct mutex cheeze_mutex;
void cheeze_chr_cleanup_module(void);
int cheeze_chr_init_module(void);

// queue.c
extern struct cheeze_req *reqs;
uint64_t cheeze_push(struct request *rq, struct cheeze_req **req);
struct cheeze_req *cheeze_peek(void);
void cheeze_pop(int id);
void cheeze_move_pop(int id);
void cheeze_queue_init(void);
void cheeze_queue_exit(void);

//shm.c
extern void *cheeze_data_addr[2];
void __exit shm_exit(void);
int send_req (struct cheeze_req *req, int id, uint64_t seq);
static inline void *get_buf_addr(int id) {
	int idx = id / ITEMS_PER_HP;
	return cheeze_data_addr[idx] + (id * CHEEZE_BUF_SIZE);
}

#endif

#endif
