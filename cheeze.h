// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#ifndef __CHEEZE_H
#define __CHEEZE_H

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
	int id; // Set by cheeze
	int buf_len; // Set by koo
	char *buf; // Set by koo
	int ubuf_len; // Set by bom
	char __user *ubuf; // Set by bom
	char *ret_buf; // Set by koo (Could be NULL)
} __attribute__((aligned(8), packed));

#ifdef __KERNEL__

struct cheeze_req {
	struct completion acked; // Set by cheeze
	struct cheeze_req_user *user; // Set by koo, needs to be freed by koo
};

// blk.c
void cheeze_io(struct cheeze_req_user *user); // Called by koo
extern struct class *cheeze_chr_class;
// extern struct mutex cheeze_mutex;
void cheeze_chr_cleanup_module(void);
int cheeze_chr_init_module(void);

// queue.c
extern struct cheeze_req *reqs;
int cheeze_push(struct cheeze_req_user *user);
struct cheeze_req *cheeze_peek(void);
void cheeze_pop(int id);
void cheeze_queue_init(void);

#endif

#endif
