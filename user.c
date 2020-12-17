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

#define unlikely(x)     __builtin_expect(!!(x), 0)

enum req_opf {
	/* read sectors from the device */
	REQ_OP_READ = 0,
	/* write sectors to the device */
	REQ_OP_WRITE = 1,
	/* flush the volatile write cache */
	REQ_OP_FLUSH = 2,
	/* discard sectors */
	REQ_OP_DISCARD = 3,
	/* get zone information */
	REQ_OP_ZONE_REPORT = 4,
	/* securely erase sectors */
	REQ_OP_SECURE_ERASE = 5,
	/* seset a zone write pointer */
	REQ_OP_ZONE_RESET = 6,
	/* write the same sector many times */
	REQ_OP_WRITE_SAME = 7,
	/* write the zero filled sector many times */
	REQ_OP_WRITE_ZEROES = 9,

	/* SCSI passthrough using struct scsi_request */
	REQ_OP_SCSI_IN = 32,
	REQ_OP_SCSI_OUT = 33,
	/* Driver private requests */
	REQ_OP_DRV_IN = 34,
	REQ_OP_DRV_OUT = 35,

	REQ_OP_LAST,
};

struct cheeze_req_user {
	// Aligned to 32B
	int id;
	int op;
	char *buf;
	unsigned int pos;	// sector_t
	unsigned int len;
	unsigned int pad[2];
};

#define STRIPE_SIZE (128 * 1024)
#define QUEUE_DEPTH (32 * 2 * 1024 * 1024 / 4096)

// Warning, output is static so this function is not reentrant
static const char *humanSize(uint64_t bytes)
{
	static char output[200];

	char *suffix[] = { "B", "KiB", "MiB", "GiB", "TiB" };
	char length = sizeof(suffix) / sizeof(suffix[0]);

	int i = 0;
	double dblBytes = bytes;
	if (bytes > 1024) {
		for (i = 0; (bytes / 1024) > 0 && i < length - 1;
		     i++, bytes /= 1024)
			dblBytes = bytes / 1024.0;
	}

	sprintf(output, "%.02lf %s", dblBytes, suffix[i]);

	return output;
}

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

static inline uint64_t ts_to_ns(struct timespec *ts)
{
	return ts->tv_sec * (uint64_t) 1000000000L + ts->tv_nsec;
}

#define POS (req.pos * 4096UL)

static void *ptr_align(void const *ptr, size_t alignment)
{
	char const *p0 = ptr;
	char const *p1 = p0 + alignment - 1;
	return (void *)(p1 - (size_t)p1 % alignment);
}

#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

static char __tmpbuf[2 * 1024 * 1024 + PAGE_SIZE];
static char *tmpbuf = __tmpbuf;
int main()
{
	int ret;
	int chrfd, copyfd[2];
	ssize_t r;
	struct cheeze_req_user req;
	unsigned int stripe_in, i, j, loop;

	struct io_uring ring;
	struct io_uring_sqe *sqe[QUEUE_DEPTH];
	struct io_uring_cqe *cqe;

	ssize_t pos[2], moving_pos[2];

	chrfd = open("/dev/cheeze_chr", O_RDWR);
	if (chrfd < 0) {
		perror("Failed to open /dev/cheeze_chr");
		return 1;
	}

	copyfd[0] = open("/tmp/vda", O_RDWR);
	copyfd[1] = open("/tmp/vdb", O_RDWR);
	for (i = 0; i < 2; i++) {
		if (copyfd[i] < 0) {
			perror("Failed to open file");
			exit(1);
		}
	}

	/* Initialize io_uring */
	io_uring_queue_init(QUEUE_DEPTH, &ring, 0);

	tmpbuf = ptr_align(tmpbuf, PAGE_SIZE);

	while (1) {
		r = read(chrfd, &req, sizeof(struct cheeze_req_user));
		if (r < 0)
			break;

		if (req.op != REQ_OP_READ && req.op != REQ_OP_WRITE) {
			write(chrfd, &req, sizeof(struct cheeze_req_user));
			continue;
		}

/*
		printf("req[%d]\n"
			"  pos=%d\n"
			"  len=%d\n",
				req.id, req.pos, req.len);
*/

		stripe_in = POS / STRIPE_SIZE;

		pos[0] = ((stripe_in / 2 + (stripe_in % 2 ? 1 : 0)) * STRIPE_SIZE) + (POS - stripe_in * STRIPE_SIZE) * (stripe_in % 2 ? 0 : 1);
		pos[1] = (stripe_in / 2 * STRIPE_SIZE) + (POS - stripe_in * STRIPE_SIZE) * (stripe_in % 2 ? 1 : 0);
		moving_pos[0] = pos[0];
		moving_pos[1] = pos[1];

		req.buf = tmpbuf;

		if (req.op == REQ_OP_WRITE)
			write(chrfd, &req, sizeof(struct cheeze_req_user));

		loop = req.len / 4096;
		for (i = 0; i < loop; i++) {
			j = ((POS + i * 4096) / STRIPE_SIZE) % 2;

			sqe[i] = io_uring_get_sqe(&ring);
			if (unlikely(!sqe[i])) {
				perror("Could not get SQE");
				return 1;
			}

			if (req.op == REQ_OP_READ)
				io_uring_prep_read(sqe[i], copyfd[j],
						   tmpbuf + (i * 4096), 4096,
						   moving_pos[j]);
			else
				io_uring_prep_write(sqe[i], copyfd[j],
						    tmpbuf + (i * 4096), 4096,
						    moving_pos[j]);
			moving_pos[j] += 4096;
		}
		ret = io_uring_submit(&ring);

		// Blocking
		ret = io_uring_wait_cqe_nr(&ring, &cqe, loop);
		if (unlikely(ret != 0)) {
			fprintf(stderr, "io_uring(%s:%d) failed: %d(%s)\n", __FILE__, __LINE__, ret, strerror(ret * -1));
			return 1;
		}
		for (i = 0; i < loop; i++) {
			do {
				ret = io_uring_peek_cqe(&ring, &cqe);
				if (unlikely(ret != 0)) {
					fprintf(stderr, "io_uring(%s:%d) failed: %d(%s)\n", __FILE__, __LINE__, ret, strerror(ret * -1));
					//return 1;
				}
			} while (ret != 0);
			/*
			if (unlikely(cqe->res < 0)) {
				fprintf(stderr, "io_uring(%s:%d) failed: %s\n", __FILE__, __LINE__, strerror(cqe->res * -1));
				return 1;
			}
			*/
			io_uring_cqe_seen(&ring, cqe);
		}

		if (req.op == REQ_OP_READ)
			write(chrfd, &req, sizeof(struct cheeze_req_user));
	}

	return 0;
}
