// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#ifndef __CHEEZE_H
#define __CHEEZE_H

#define CHEEZE_QUEUE_SIZE 256

#define OP_READ 0
#define OP_WRITE 1

struct cheeze_req {
	int rw;
	unsigned int index;
	unsigned int offset;
	unsigned int size;
	unsigned int addr;
};

// blk.c
void cheeze_chr_cleanup_module(void);
int cheeze_chr_init_module(void);

// queue.c
extern struct cheeze_req *reqs;
void cheeze_push(const int rw,
		 const unsigned int index,
		 const unsigned int offset,
		 const unsigned int size,
		 const unsigned int addr);
struct cheeze_req *cheeze_pop(void);

#endif
