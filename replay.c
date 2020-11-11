// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>

struct cheeze_req_user {
	int id;
	int op;
	unsigned int pos; // sector_t
	unsigned int len;
	uint32_t crc;
} __attribute__((aligned(8), packed));

#define TRACE_TARGET "/trace"

int main() {
	int dumpfd;
	struct cheeze_req_user ureq;

	dumpfd = open(TRACE_TARGET, O_RDONLY);
	if (dumpfd < 0) {
		perror("Failed to open " TRACE_TARGET);
		return 1;
	}

	while (read(dumpfd, &ureq, sizeof(ureq)) == sizeof(ureq)) {
		printf("id=%d\n    op=%d\n    pos=%u\n    len=%u\n    crc=0x%x\n\n", ureq.id, ureq.op, ureq.pos, ureq.len, ureq.crc);
	}

	close(dumpfd);

	return 0;
}
