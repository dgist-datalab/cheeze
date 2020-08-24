// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#ifndef __CHEEZE_H
#define __CHEEZE_H

#define CHEEZE_QUEUE_SIZE 1024
#define CHEEZE_CHR_MAJOR 235

#define OP_READ 0
#define OP_WRITE 1

struct cheeze_req {
	int rw;
	volatile int acked;
	unsigned int index;
	unsigned int offset;
	unsigned int size;
	unsigned long id;
	void *addr;
	void *user_buf;
};

// blk.c
extern struct class *cheeze_chr_class;
// extern struct mutex cheeze_mutex;

// chr.c
void cheezer_cleanup_module(void);
int cheezer_init_module(void);
void cheezew_cleanup_module(void);
int cheezew_init_module(void);

// queue.c
extern struct cheeze_req *reqs;
unsigned long cheeze_push(const int rw,
		 const unsigned int index,
		 const unsigned int offset,
		 const unsigned int size,
		 void *addr);
struct cheeze_req *cheeze_pop(void);

#endif
