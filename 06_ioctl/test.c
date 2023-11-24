#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>

#include "chardriver_ioctl.h"

void ioctl_write(int fd, char* message)
{
    struct ioctl_data data = { strlen(message), message };
    if (ioctl(fd, DEV_IOC_WRITE, &data))
        printf("Write failed\n");
}

void ioctl_read(int fd)
{
    struct ioctl_data data;
    if (ioctl(fd, DEV_IOC_READ, &data))
        printf("Read failed\n");

    printf("Message: %s, size: %li\n", data.buffer, data.size);
}

void ioctl_get_nth_byte(int fd, int n)
{
    int index = n;
    if (ioctl(fd, DEV_IOC_READ_NTH_BYTE, &n))
        printf("Read failed\n");
    
    printf("Nth byte of message: %c, index: %i", n, index);
}

void ioctl_reset(int fd)
{
    if (ioctl(fd, DEV_IOC_RESET))
        printf("Reset failed\n");
}

int main()
{
    int fd = open("/dev/chardevice0", O_RDWR);

    char message[] = "ABCDEF";

    ioctl_reset(fd);
    ioctl_write(fd, message);
    ioctl_read(fd);

    close(fd);

    return 0;
}
