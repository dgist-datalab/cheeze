// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <zlib.h>

#define __user
#include "cheeze.h"

#define TEST_STRING "kooisdabest"

int main() {
	int chrfd;
	char *mem;
	ssize_t r;
	struct cheeze_req_user req;

	chrfd = open("/dev/cheeze_chr", O_RDWR);
	if (chrfd < 0) {
		perror("Failed to open /dev/cheeze_chr");
		return 1;
	}

	mem = malloc(2 * 1024 * 1024); // 2MB
	if (!mem) {
		perror("Failed to allocate memory");
		return 1;
	}

	req.buf = mem;

	while (1) {
		r = read(chrfd, &req, sizeof(struct cheeze_req_user));
		if (r < 0)
			break;

		printf("req[%d]'s length = %d\n", req.id, req.buf_len);
		//for (int i = 0; i < req.buf_len; i++)
		//	printf("%d ", mem[i]);
		//printf("\n");
		// printf("req[%d]'s CRC32: 0x%lx\n", req.id, crc32(0, (unsigned char *)mem, req.buf_len));

		if (req.ret_buf) {
			req.ubuf_len = strlen(TEST_STRING) + 1;
			req.ubuf = TEST_STRING;
		}

		write(chrfd, &req, sizeof(struct cheeze_req_user));
	}

	return 0;
}
