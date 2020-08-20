// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <stdio.h>
#include <string.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>

struct cheeze_req {
	int rw;
	int acked;
	unsigned int index;
	unsigned int offset;
	unsigned int size;
	unsigned long id;
	void *addr;
};

#define COPY_TARGET "/mnt/home/arter97/todo"

int main() {
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

	copyfd = open(COPY_TARGET, O_RDONLY);
	if (copyfd < 0) {
		perror("Failed to open " COPY_TARGET);
		return 1;
	}

	while ((r = read(chrfd, req, sizeof(struct cheeze_req))) >= 0) {
		printf("New req[%lu]\n"
			"  rw=%d\n"
			"  index=%u\n"
			"  offset=%u\n"
			"  size=%u\n"
			"  addr=%p\n",
				req->id, req->rw, req->index, req->offset, req->size, req->addr);

		// printf("Before: ");
		// memcpy(buf, mem + req->addr, req->size);
		// write(1, buf, req->size);

		// read(copyfd, mem + req->addr, req->size);
		read(copyfd, buf + sizeof(struct cheeze_req), req->size);
		// memcpy(mem + req->addr, buf, req->size);

		// printf("\nAfter: ");
		// memcpy(buf, mem + req->addr, req->size);
		// write(1, buf, req->size);
		// printf("\n");

		write(chrfd, buf, sizeof(struct cheeze_req) + req->size);
	}

	return 0;
}
