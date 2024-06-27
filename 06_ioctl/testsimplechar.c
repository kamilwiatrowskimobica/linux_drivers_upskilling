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

#define BUFF_LEN 256
#define DEVICE_NAME "/dev/mk_mob_0"
#define TEST_COUNT 3000

int main()
{
   char *buff = "1234TEST_PASSED";

   struct mk_mob_ioctl_data ioctl_data = {
      .data = buff,
      .off  = 4,
      .size = strlen(buff)
   };

   int fd = open(DEVICE_NAME, O_RDWR);
   if (fd < 0) {
      printf("Failed to open the device for read/write\n");
      return -1;
   }

   if (ioctl(fd, MK_MOB_IOC_RESET) < 0) {
      printf("MOBDEV_IOC_RESET failed, err=%d\n", errno);
   }

   // Write the string with offest to the device
   if (ioctl(fd, MK_MOB_IOC_SET, (struct mk_mob_ioctl_data*) &ioctl_data) < 0) {
      printf("MOBDEV_IOC_SET failed, err=%d\n", errno);
   }

   char ret_buff[sizeof("1234TEST_PASSED")] = "";

   ioctl_data.off = 8;
   ioctl_data.data = ret_buff;
   if (ioctl(fd, MK_MOB_IOC_GET, (struct mk_mob_ioctl_data*) &ioctl_data) < 0) {
      printf("MOBDEV_IOC_GET failed, err=%d\n", errno);
   } 

   printf("%s\n", ret_buff);

   close(fd);

   return 0;
}