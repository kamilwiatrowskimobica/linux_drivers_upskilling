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

#define DEVICE "/dev/block_disk"

static void test(int fd, int sector)
{
    int i;
    char write_buffer[SECTOR_SIZE], read_buffer[SECTOR_SIZE];

    for (i = 0; i < sizeof(write_buffer) / sizeof(write_buffer[0]); i++)
        write_buffer[i] = rand() % 256;

    lseek(fd, sector * SECTOR_SIZE, SEEK_SET);
    write(fd, write_buffer, sizeof(write_buffer));

    fsync(fd);

    lseek(fd, sector * SECTOR_SIZE, SEEK_SET);
    read(fd, read_buffer, sizeof(read_buffer));

    printf("test sector %d", sector);

    if (memcmp(write_buffer, read_buffer, sizeof(read_buffer)) == 0)
        printf("test passed");
    else
        printf("test failed");
}

int main()
{
    int fd, i;

    fd = open(DEVICE, O_RDWR);
    if (fd < 0)
    {
        exit(EXIT_FAILURE);
    }

    srand(time(NULL));
    
    for (i = 0; i < NR_SECTORS; i++)
        test(fd, i);

    close(fd);

    return 0;
}
