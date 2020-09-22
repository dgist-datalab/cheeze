#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <zlib.h>

#define PHYS_ADDR 0x3280000000
#define TOTAL_SIZE (1024L * 1024L * 1024L) // 1 GB

#define BUF_SIZE (1024 * 1024) // 1 MB
struct pingpong_fmt {
	volatile int ready;
	unsigned char buf[BUF_SIZE];
} __attribute__((aligned(8), packed));

static void *mem;

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
	mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, addr);
	if (mem == MAP_FAILED) {
		perror("Failed to mmap plain device path");
		exit(1);
	}

	close(fd);
}

static int random_fd;

static void loop()
{
	struct pingpong_fmt *pp;
	pp = mem;

	for (;;) {
		while (pp->ready) usleep(1 * 1000);
		read(random_fd, pp->buf, BUF_SIZE);
		printf("CRC: 0x%lx\n", crc32(0, pp->buf, BUF_SIZE));
		pp->ready = 1;
	}
}

int main()
{
	mem_init();

	random_fd = open("/dev/urandom", O_RDONLY);
	if (random_fd < 0) {
		perror("Failed to open /dev/urandom");
		exit(1);
	}

	loop();
}


















