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
#include <sys/uio.h>

#include <liburing.h>

static void *ptr_align(void const *ptr, size_t alignment)
{
	char const *p0 = ptr;
	char const *p1 = p0 + alignment - 1;
	return (void *) (p1 - (size_t) p1 % alignment);
}

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#define QUEUE_DEPTH 1

static char __tmpbuf[2 * 1024 * 1024 + PAGE_SIZE];
static char *tmpbuf = __tmpbuf;
int main()
{
	int ret, i, j;
	int chrfd, copyfd[2];
	ssize_t r;

	struct io_uring ring[2];
	struct io_uring_sqe *sqe[2];
	struct io_uring_cqe *cqe[2];
	ssize_t pos[2];

	copyfd[0] = open("/home/arter97/Xorg.0.log", O_RDWR);
	if (copyfd[0] < 0) {
		perror("Failed to open file");
		exit(1);
	}

	j = 0;
	/* Initialize io_uring */
	io_uring_queue_init(QUEUE_DEPTH, &ring[j], 0);

	tmpbuf = ptr_align(tmpbuf, PAGE_SIZE);

	sqe[j] = io_uring_get_sqe(&ring[j]);
	if (!sqe[j]) {
		perror("Could not get SQE");
		return 1;
	}

	io_uring_prep_read(sqe[j], copyfd[j], tmpbuf, 4096, 0);//pos[j]);
	io_uring_submit(&ring[j]);

	ret = io_uring_wait_cqe(&ring[j], &cqe[j]);
	if (ret < 0) {
		perror("io_uring_wait_cqe");
		return 1;
	}
	if (cqe[j]->res < 0) {
		fprintf(stderr, "io_uring(%s:%d) failed: %d: %s\n", __FILE__, __LINE__, cqe[j]->res, strerror(cqe[j]->res * -1));
		return 1;
	}
	io_uring_cqe_seen(&ring[j], cqe[j]);

	io_uring_queue_exit(&ring[j]);

	return 0;
}
