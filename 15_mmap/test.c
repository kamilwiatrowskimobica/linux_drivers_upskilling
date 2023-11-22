#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define NPAGES 16
#define PROC_ENTRY_NAME "mmap_driver_seq"
#define MAP_NAME "mmap_device"
#define MMAP_DEV "/dev/" MAP_NAME
#define PROC_ENTRY_FILE "/proc" PROC_ENTRY_NAME

void test_mem(unsigned char* addr, unsigned char v1, unsigned char v2, unsigned char v3, unsigned char v4)
{
    int i;

    printf("%d", getpagesize());

    for (i = 0; i < NPAGES * getpagesize(); i += getpagesize())
    {
        if (addr[i] != v1 || addr[i + 1] != v2 || addr[i + 2] != v3 || addr[i + 3] != v4)
            printf("0x%x 0x%x 0x%x 0x%x\n", addr[i], addr[i + 1], addr[i + 2], addr[i + 3]);
        else
            printf("matched %p %p %p %p\n", &addr[i], &addr[i + 1], &addr[i + 2], &addr[i + 3]);
    }
}

int test_read_write(int fd, unsigned char* mmap_addr)
{
    unsigned char* local_addr;
    int len = NPAGES * getpagesize(), i;

    printf("write test\n");
    local_addr = malloc(len);
    if (!local_addr)
    {
        printf("error during memory allocation");
        return -1;
    }
    
    printf("local address = %p\n", local_addr);
    printf("mmap address = %p\n", mmap_addr);

    memset(local_addr, 0, len);
    for (i = 0; i < NPAGES * getpagesize(); i += getpagesize())
    {
        local_addr[i] = 0xA0;
        local_addr[i + 1] = 0xB0;
        local_addr[i + 2] = 0xC0;
        local_addr[i + 3] = 0xD0;
    }

    write(fd, local_addr, len);
    test_mem(mmap_addr, 0xA0, 0xB0, 0xC0, 0xD0);

    return 0;
}

int main()
{
    int fd, test = 3;
    unsigned char* addr;
    int len = NPAGES * getpagesize();
    int i;
    unsigned long usage_before_mmap, usage_after_mmap;
    const int test3_size = 20 * 1024 * 1024;

    fd = open("/dev/mmap_device", O_RDWR | O_SYNC);
    if (fd < 0)
        printf("falied to open file");

    addr = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
    printf("mmap addr = %p\n", addr);

    if (addr == MAP_FAILED)
        return -1;
    
    if (test == 1)
        test_mem(addr, 0xAA, 0xBB, 0xCC, 0xDD);
    else if (test == 2)
        test_read_write(fd, addr);

    close(fd);

    return 0;
}
