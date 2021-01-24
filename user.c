// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Park Ju Hyung
 */

#include <errno.h>
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
	int id;
	int op;
	char *buf;
	unsigned int pos;	// sector_t
	unsigned int len;
	long long unsigned int kaddr;
};

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

static inline uint64_t ts_to_ns(struct timespec* ts) {
	return ts->tv_sec * (uint64_t)1000000000L + ts->tv_nsec;
}

#define PHYS_ADDR 0x0
#define TOTAL_SIZE (8 * 1024L * 1024L * 1024L) // 4 GB

static void *phys_mem;

static void mem_init()
{
	uint64_t pagesize, addr, len;
	int fd;

	fd = open("/dev/mem", O_RDWR);
	if (fd == -1) {
		perror("Failed to open /dev/mem");
		exit(1);
	}

	pagesize = getpagesize();
	addr = PHYS_ADDR & (~(pagesize - 1));
	len = (PHYS_ADDR & (pagesize - 1)) + TOTAL_SIZE;
	phys_mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr);
	if (phys_mem == MAP_FAILED) {
		perror("Failed to mmap plain device path");
		exit(1);
	}

	close(fd);
}

int main(int argc, char *argv[]) {
	int chrfd, copyfd;
	char *mem;
	ssize_t r;
	struct cheeze_req_user req;

	chrfd = open("/dev/cheeze_chr", O_RDWR);
	if (chrfd < 0) {
		perror("Failed to open /dev/cheeze_chr");
		return 1;
	}

	copyfd = open(argv[1], O_RDWR);
	if (copyfd < 0) {
		fprintf(stderr, "Failed to open %s: %s\n", argv[1], strerror(errno));
		return 1;
	}

	mem = mmap(NULL, fdlength(copyfd), PROT_READ | PROT_WRITE, MAP_SHARED, copyfd, 0);
	if (mem == MAP_FAILED) {
		perror("Failed to mmap copy path");
		return 1;
	}

	mem_init();

	close(copyfd);

	while (1) {
		r = read(chrfd, &req, sizeof(struct cheeze_req_user));
		if (r < 0)
			break;

		req.buf = mem + (req.pos * 4096UL);

		printf("req[%d]\n"
			"  pos=%u\n"
			"  len=%u\n"
			"  kaddr=%llu\n"
			"  pos=%p\n",
				req.id, req.pos, req.len, req.kaddr, req.buf);

		switch (req.op) {
		case REQ_OP_DISCARD:
			printf("Trimming offset %s", humanSize(req.pos * 4096UL));
			printf(" with length %s\n", humanSize(req.len));
			memset(req.buf, 0, req.len);
			break;
		case REQ_OP_READ:
			memcpy(phys_mem + req.kaddr, req.buf, req.len);
			break;
		case REQ_OP_WRITE:
			memcpy(req.buf, phys_mem + req.kaddr, req.len);
			break;
		}

		// Sanity check
		// memset(req.buf, 0, req.size);

		write(chrfd, &req, sizeof(struct cheeze_req_user));
	}

	return 0;
}
