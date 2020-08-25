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

struct cheeze_req {
	int rw;
	unsigned int index;
	unsigned int offset;
	unsigned int size;
	int id;
	void *addr;
	void *user_buf;
	char acked[32]; // struct completion acked;
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
	struct timespec start, end;
	uint64_t trans_time = 0;
	struct sysinfo info;
	int memfd, chrfd, copyfd;
	int ret;
	char *mem;
	char buf[4096 * 4];
	ssize_t r;
	struct cheeze_req *req = (struct cheeze_req *)buf;

/*
	ret = sysinfo(&info);
	if (ret < 0) {
		perror("Failed to query sysinfo()");
		return 1;
	}

	memfd = open("/dev/mem", O_RDWR | O_SYNC);
	if (memfd < 0) {
		perror("Failed to open /dev/mem");
		return 1;
	}

	mem = mmap(NULL, info.totalram, PROT_READ | PROT_WRITE, MAP_SHARED, memfd, 0);
	if (mem == MAP_FAILED) {
		perror("Failed to mmap plain device path");
		return 1;
	}
	close(memfd);
*/

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

/*
	mem = malloc(fdlength(copyfd));
	printf("malloc'ed\n");
	read(copyfd, mem, fdlength(copyfd));
	printf("read'ed\n");
*/
	mem = mmap(NULL, fdlength(copyfd), PROT_READ | PROT_WRITE, MAP_SHARED, copyfd, 0);
	if (mem == MAP_FAILED) {
		perror("Failed to mmap copy path");
		return 1;
	}

	close(copyfd);

	while (1) {
//	while ((r = read(chrfd, req, sizeof(struct cheeze_req))) >= 0) {
		//clock_gettime(CLOCK_MONOTONIC_RAW, &start);
		r = read(chrfd, req, sizeof(struct cheeze_req));
		//clock_gettime(CLOCK_MONOTONIC_RAW, &end);
		//trans_time += (ts_to_ns(&end) - ts_to_ns(&start));
		//printf("Transmission time: %ld.%.9ld\n", trans_time / 1000000000L, trans_time % 1000000000L);
		if (r < 0)
			break;

/*
		printf("New req[%lu]\n"
			"  rw=%d\n"
			"  index=%u\n"
			"  offset=%u\n"
			"  size=%u\n"
			"  addr=%p\n",
				req->id, req->rw, req->index, req->offset, req->size, req->addr);
*/

		// printf("Before: ");
		// memcpy(buf, mem + req->addr, req->size);
		// write(1, buf, req->size);

		// read(copyfd, mem + req->addr, req->size);
		// read(copyfd, buf + sizeof(struct cheeze_req), req->size);
		// memcpy(mem + req->addr, buf, req->size);

		// printf("\nAfter: ");
		// memcpy(buf, mem + req->addr, req->size);
		// write(1, buf, req->size);
		// printf("\n");

		req->user_buf = mem + (req->size * req->index) + req->offset;

		// Sanity check
		// memset(req->user_buf, 0, req->size);

		//clock_gettime(CLOCK_MONOTONIC_RAW, &start);
		write(chrfd, req, sizeof(struct cheeze_req));
		//clock_gettime(CLOCK_MONOTONIC_RAW, &end);
		//trans_time += (ts_to_ns(&end) - ts_to_ns(&start));
		//printf("Write time: %ld.%.9ld\n", trans_time / 1000000000L, trans_time % 1000000000L);
	}

	return 0;
}
