#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "kmap/mob_kmmap.h"

#define MMAP_DEV "/dev/" MAP_NAME
#define PROC_ENTRY_FILE "/proc/" PROC_ENTRY_NAME

void test_mem(unsigned char *addr, unsigned char v1, unsigned char v2,
	      unsigned char v3, unsigned char v4)
{
	int i;

	for (i = 0; i < NPAGES * getpagesize(); i += getpagesize()) {
		if (addr[i] != v1 || addr[i + 1] != v2 || addr[i + 2] != v3 ||
		    addr[i + 3] != v4)
			printf("0x%x 0x%x 0x%x 0x%x\n", addr[i], addr[i + 1],
			       addr[i + 2], addr[i + 3]);
		else
			printf("matched %p %p %p %p\n", &addr[i], &addr[i + 1],
			       &addr[i + 2], &addr[i + 3]);
	}
}

int test_read_write(int fd, unsigned char *mmap_addr)
{
	unsigned char *local_addr;
	int len = NPAGES * getpagesize(), i;

	printf("\nWrite test ...\n");
	local_addr = malloc(len);
	if (!local_addr)
		return -1;
	printf("local addr = %p\n", local_addr);
	printf("mmap_addr addr = %p\n", mmap_addr);

	memset(local_addr, 0, len);
	for (i = 0; i < NPAGES * getpagesize(); i += getpagesize()) {
		local_addr[i] = 0xa0;
		local_addr[i + 1] = 0xb0;
		local_addr[i + 2] = 0xc0;
		local_addr[i + 3] = 0xd0;
	}

	write(fd, local_addr, len);
	test_mem(mmap_addr, 0xa0, 0xb0, 0xc0, 0xd0);

	printf("\nRead test ...\n");
	memset(local_addr, 0, len);
	read(fd, local_addr, len);
	test_mem(local_addr, 0xa0, 0xb0, 0xc0, 0xd0);

	return 0;
}

static int show_mem_usage(void)
{
	int fd, ret;
	char buf[40];
	unsigned long mem_usage;

	fd = open(PROC_ENTRY_FILE, O_RDONLY);
	if (fd < 0) {
		perror("open " PROC_ENTRY_FILE);
		ret = fd;
		goto out;
	}

	ret = read(fd, buf, sizeof buf);
	if (ret < 0)
		goto no_read;

	sscanf(buf, "%lu", &mem_usage);
	buf[ret] = 0;

	printf("Memory usage: %lu\n", mem_usage);

	ret = mem_usage;
no_read:
	close(fd);
out:
	return ret;
}

int main(int argc, const char **argv)
{
	int fd, test = 1;
	unsigned char *addr;
	int len = NPAGES * getpagesize();
	int i;
	unsigned long usage_before_mmap, usage_after_mmap;
	const int test_3_size = 20 * 1024 * 1024;

	if (argc > 1)
		test = atoi(argv[1]);

	fd = open(MMAP_DEV, O_RDWR | O_SYNC);
	if (fd < 0) {
		return -1;
	}
	addr = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
	printf("mmap addr = %p\n", addr);

	if (addr == MAP_FAILED) {
		return -1;
	}

	if (test == 1) {
		test_mem(addr, 0xaa, 0xbb, 0xcc, 0xdd);
	} else if (test == 2) {
		test_read_write(fd, addr);
	} else if (test == 3) {
		sleep(30);
		usage_before_mmap = show_mem_usage();

		addr = mmap(NULL, test_3_size, PROT_READ,
			    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
		if (addr == MAP_FAILED)
			return -1;

		usage_after_mmap = show_mem_usage();
		printf("mmaped :%lu MB\n",
		       (usage_after_mmap - usage_before_mmap) >> 20);

		sleep(30);

		munmap(addr, test_3_size);
	}

	close(fd);

	return 0;
}