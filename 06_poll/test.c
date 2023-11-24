#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <poll.h>
#include <assert.h>
#include <sys/ioctl.h>

#include "chardriver_ioctl.h"

int main()
{
    const char* input = "BBBBBBBBBB";
    char output[20];
    int fd1, fd2;
    struct pollfd pfds[1];
    int result;

    fd1 = open("/dev/chardevice0", O_RDWR | O_NONBLOCK);

    if (fd1 < 0)
    {
        printf("file not opened");
        exit(-1);
    }

    pfds[0].fd = fd1;
    pfds[0].events = (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);

    ioctl(fd1, DEV_IOC_RESET);

    int counter = 0;

    while (1)
    {
        sleep(3);
        result = poll(pfds, 1, 900);

        if (result < 0)
        {
            printf("poll error");
            assert(0);
        }

        for (int i = 0; i != 1; i++)
        {
            if ((pfds[i].revents & POLLIN) == POLLIN)
            {
                printf("read from chardevice%i\n", i);
                read(pfds[i].fd, output, 20);
                printf("Message: %s\n", output);
            }

            if ((pfds[i].revents & POLLOUT) == POLLOUT)
            {
                printf("write to chardevice%i\n", i);
                write(pfds[i].fd, input, strlen(input));
            }
        }
    }

    return 0;
}
