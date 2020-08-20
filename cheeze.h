// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#ifndef __CHEEZE_H
#define __CHEEZE_H

struct cheeze_req {
	int rw;
	unsigned int index;
	unsigned int offset;
	unsigned int size;
	unsigned int addr;
};

void cheeze_chr_cleanup_module(void);
int cheeze_chr_init_module(void);

#endif
