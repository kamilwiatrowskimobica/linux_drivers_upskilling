#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include "../mobi_ram_disk.h"


#define BLKDEV_NAME	"/dev/" MOBI_DISK_NAME
#define MODULE_NAME "mobi_ram_disk"
#define max_elem_value(elem) (1 << 8*sizeof(elem))

static unsigned char buffer[KERNEL_SECTOR_SIZE];
static unsigned char buffer_copy[KERNEL_SECTOR_SIZE];


static void test_sector(int fd, size_t sector)
{
	int i;

	for (i = 0; i < sizeof(buffer) / sizeof(buffer[0]); i++)
		buffer[i] = rand() % max_elem_value(buffer[0]);

	lseek(fd, sector * KERNEL_SECTOR_SIZE, SEEK_SET);
	write(fd, buffer, sizeof(buffer));

	fsync(fd);

	lseek(fd, sector * KERNEL_SECTOR_SIZE, SEEK_SET);
	read(fd, buffer_copy, sizeof(buffer_copy));

	printf("test sector %3d ... ", sector);
	if (memcmp(buffer, buffer_copy, sizeof(buffer_copy)) == 0)
		printf("passed\n");
	else
		printf("failed\n");
}

int main(void)
{
	int fd;
	size_t i;
	int back_errno;
	char cmd[128] = {0};


	printf("insmod ../" MODULE_NAME ".ko\n");
	if (system("insmod ../" MODULE_NAME ".ko\n")) {
		fprintf(stderr, "insmod failed\n");
		exit(EXIT_FAILURE);
	}

	sleep(1);

	back_errno = snprintf(cmd, sizeof(cmd), "mknod %s b %d %d\n", BLKDEV_NAME, MOBI_BLOCK_MAJOR, MOBI_BLOCK_MINOR);
	if (back_errno <= 0) {
		perror("snprintf");
		fprintf(stderr, "errno is %d\n", back_errno);
		exit(EXIT_FAILURE);
	}

	printf("%s", cmd);
	system(cmd);
	
	sleep(1);

	printf("Try to open: %s\n", BLKDEV_NAME);

	fd = open(BLKDEV_NAME, O_RDWR);
	if (fd < 0) {
		back_errno = errno;
		perror("open");
		fprintf(stderr, "errno is %d\n", back_errno);
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));
	for (i = 0; i < NR_SECTORS; i++)
		test_sector(fd, i);

	close(fd);

	return 0;
}
