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
#include <poll.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define DEVICE_NAME "/dev/mk_mob_0"

int main(int argc, char **argv)
{
   struct pollfd pfd;
   
   int fd = open(DEVICE_NAME, O_RDWR);
   if (fd < 0) {
      printf("Failed to open the device for read/write\n");
      return -1;
   }

   if (argc == 2 && strcmp(argv[1], "true") == 0) {
      if (ioctl(fd, MK_MOB_IOC_ENABLE_WRITE) < 0) {
         printf("MK_MOB_IOC_ENABLE_WRITE failed, err=%d\n", errno);
      }
   }
   else if (argc == 2 && strcmp(argv[1], "false") == 0) {
      if (ioctl(fd, MK_MOB_IOC_DISABLE_WRITE) < 0) {
         printf("MK_MOB_IOC_DISABLE_WRITE failed, err=%d\n", errno);
      }
   }
   else {
      printf("wrong argument value/format\n");
   }

   close(fd);

   return 0;
}