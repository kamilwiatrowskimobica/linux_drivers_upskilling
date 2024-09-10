/**
 * @file   testsimplechar.c
 * @author Michal Kozikowski
 * @date   10 June 2024
 * @version 0.1
*/

#include "mk_mob.h"

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_NAME "/dev/mk_mob_0"

int main()
{

   int fd = open(DEVICE_NAME, O_RDWR);
   if (fd < 0) {
      printf("Failed to open the device for read/write\n");
      return -1;
   }

    while (1)
    {        
        if (ioctl(fd, MK_MOB_IOC_LED_ON) < 0) {
            printf("MOBDEV_IOC_LED_ON failed, err=%d\n", errno);
            break;
        }
        usleep(500000);
        if (ioctl(fd, MK_MOB_IOC_LED_OFF) < 0) {
            printf("MOBDEV_IOC_LED_OFF failed, err=%d\n", errno);
            break;
        }
        usleep(500000);
    }

   close(fd);

   return 0;
}