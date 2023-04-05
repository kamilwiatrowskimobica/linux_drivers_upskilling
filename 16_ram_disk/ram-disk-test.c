#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>

#define NR_SECTORS 128
#define SECTOR_SIZE 512

#define DEVICE "/dev/woabdev"

static void test(int fd, int sector)
{
	int i;

	char buffer[SECTOR_SIZE], buffer_copy[SECTOR_SIZE];

	for (i = 0; i < sizeof(buffer) / sizeof(buffer[0]); i++)
		buffer[i] = rand() % 256;
		//buffer[i] = 0xC4;

	lseek(fd, sector * SECTOR_SIZE, SEEK_SET);
	write(fd, buffer, sizeof(buffer));

	fsync(fd);

	lseek(fd, sector * SECTOR_SIZE, SEEK_SET);
	read(fd, buffer_copy, sizeof(buffer_copy));

	printf("test sector %d ... ", sector);
	if (memcmp(buffer, buffer_copy, sizeof(buffer_copy)) == 0)
		printf("passed\n");
	else
		printf("failed\n");
}

int main(void)
{
	int fd, i;

	fd = open(DEVICE, O_RDWR);
	if (fd < 0) {
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));
	for (i = 0; i < NR_SECTORS; i++)
		test(fd, i);

	close(fd);

	return 0;
}