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

struct cheeze_req_user {
	int id;
	char *buf;
	unsigned int pos; // sector_t
	unsigned int len;
} __attribute__((aligned(8), packed));

#define COPY_TARGET "/tmp/vdb"

static off_t fdlength(int fd)
{
	struct stat st;
	off_t cur, ret;

	if (!fstat(fd, &st) && S_ISREG(st.st_mode))
		return st.st_size;

	cur = lseek(fd, 0, SEEK_CUR);
	ret = lseek(fd, 0, SEEK_END);
	lseek(fd, cur, SEEK_SET);

	return ret;
}

static inline uint64_t ts_to_ns(struct timespec* ts) {
	return ts->tv_sec * (uint64_t)1000000000L + ts->tv_nsec;
}

int main() {
	int chrfd, copyfd;
	char *mem;
	ssize_t r;
	struct cheeze_req_user req;

	chrfd = open("/dev/cheeze_chr", O_RDWR);
	if (chrfd < 0) {
		perror("Failed to open /dev/cheeze_chr");
		return 1;
	}

	copyfd = open(COPY_TARGET, O_RDWR);
	if (copyfd < 0) {
		perror("Failed to open " COPY_TARGET);
		return 1;
	}

	mem = mmap(NULL, fdlength(copyfd), PROT_READ | PROT_WRITE, MAP_SHARED, copyfd, 0);
	if (mem == MAP_FAILED) {
		perror("Failed to mmap copy path");
		return 1;
	}

	close(copyfd);

	while (1) {
		r = read(chrfd, &req, sizeof(struct cheeze_req_user));
		if (r < 0)
			break;

		req.buf = mem + (req.pos * 4096UL);

/*
		printf("req[%d]\n"
			"  pos=%u\n"
			"  len=%u\n"
			"mem=%p\n"
			"  pos=%p\n",
				req.id, req.pos, req.len, mem, req.buf);
*/

		// Sanity check
		// memset(req.buf, 0, req.size);

		write(chrfd, &req, sizeof(struct cheeze_req_user));
	}

	return 0;
}
