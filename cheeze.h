// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#ifndef __CHEEZE_H
#define __CHEEZE_H

#define CHEEZE_QUEUE_SIZE 1024

#define OP_READ 0
#define OP_WRITE 1

// #define DEBUG
#define DEBUG_SLEEP 1

#ifdef DEBUG
  #define msleep_dbg msleep
#else
  #define msleep_dbg(...) ((void)0)
#endif

struct cheeze_req {
	int rw;
	unsigned int index;
	unsigned int offset;
	unsigned int size;
	int id;
	void *addr;
	void *user_buf;
	struct completion acked;
} __attribute__((aligned(8), packed));

// blk.c
extern struct class *cheeze_chr_class;
// extern struct mutex cheeze_mutex;
void cheeze_chr_cleanup_module(void);
int cheeze_chr_init_module(void);

// queue.c
extern struct cheeze_req *reqs;
int cheeze_push(const int rw,
		 const unsigned int index,
		 const unsigned int offset,
		 const unsigned int size,
		 void *addr);
struct cheeze_req *cheeze_pop(void);
void cheeze_queue_init(void);

#endif
