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
	int id;
	int op;
	char *buf;
	unsigned int pos;	// sector_t
	unsigned int len;
} __attribute__ ((aligned(8), packed));

#define COPY_TARGET "/tmp/vdb"
#define STRIPE_SIZE (4 * 1024)

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

int main()
{
	int chrfd;
	int copyfd[2];
	char *mem1, mem2;
	ssize_t r;
	struct cheeze_req_user req;
	char *tmpbuf = malloc(2 * 1024 * 1024);
	unsigned int stripe_pos, spr_n = 0, lspr_s, i;

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

	while (1) {
		r = read(chrfd, &req, sizeof(struct cheeze_req_user));
		if (r < 0)
			break;

		stripe_pos = POS / STRIPE_SIZE;

		if (req.op != REQ_OP_READ && req.op != REQ_OP_WRITE)
			goto pass;

/*
		printf("req[%d]\n"
			"  pos=%d\n"
			"  len=%d\n",
				req.id, req.pos, req.len);
*/
		// req.buf = mem1 + (POS);

		if (req.len % 4096)
			printf("Unaligned: %u\n", req.len % 4096);

		spr_n = req.len / STRIPE_SIZE;
		lspr_s = req.len % STRIPE_SIZE;
		if (lspr_s)
			printf("%u %u\n", spr_n, lspr_s);

//		printf("  Seeking vda to to %lu\n", (POS / 2 / STRIPE_SIZE + (stripe_pos % 2 ? 1 : 0)) * STRIPE_SIZE);
		r = lseek(copyfd[0], (POS / 2 / STRIPE_SIZE + (stripe_pos % 2 ? 1 : 0)) * STRIPE_SIZE, SEEK_SET);
		if (r < 0)
			perror("Failed to seek - 1");
//		printf("  Seeking vdb to to %lu\n", (POS / 2 / STRIPE_SIZE + (stripe_pos % 2 ? 0 : 0)) * STRIPE_SIZE);
		r = lseek(copyfd[1], (POS / 2 / STRIPE_SIZE + (stripe_pos % 2 ? 0 : 0)) * STRIPE_SIZE, SEEK_SET);
		if (r < 0)
			perror("Failed to seek - 2");

		if (req.op == REQ_OP_READ) {
			for (i = 0; i < spr_n; i++) {
				r = read(copyfd[(i + stripe_pos) % 2],
					 tmpbuf + (i * STRIPE_SIZE),
					 STRIPE_SIZE);
				if (r != STRIPE_SIZE) {
					perror("Failed to R/W - 1");
					break;
				}
			}
		}

pass:
		req.buf = tmpbuf;
		write(chrfd, &req, sizeof(struct cheeze_req_user));

		if (req.op == REQ_OP_WRITE) {
			for (i = 0; i < spr_n; i++) {
				r = write(copyfd[(i + stripe_pos) % 2],
					  tmpbuf + (i * STRIPE_SIZE),
					  STRIPE_SIZE);
				if (r != STRIPE_SIZE) {
					perror("Failed to R/W - 1");
					break;
				}
			}
		}
	}

	return 0;
}
