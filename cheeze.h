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

#define CHEEZE_QUEUE_SIZE 4096

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
	char *buf;
	unsigned int pos; // sector_t but divided by 4096
	unsigned int len;
} __attribute__((aligned(8), packed));

struct cheeze_req {
	int ret;
	bool is_rw;
	struct request *rq;
	struct cheeze_req_user user;
	struct completion acked;
} __attribute__((aligned(8), packed));

// blk.c
extern struct class *cheeze_chr_class;
extern bool cheeze_opened;
// extern struct mutex cheeze_mutex;
void cheeze_chr_cleanup_module(void);
int cheeze_chr_init_module(void);

// queue.c
extern struct cheeze_req *reqs;
int cheeze_push(struct request *rq);
struct cheeze_req *cheeze_peek(void);
void cheeze_pop(int id);
void cheeze_queue_init(void);

#endif
