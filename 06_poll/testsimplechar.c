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
#define TEST_STRING "Hello from the user space!"

int main()
{
   struct pollfd pfd;
   int ret;
   
   int fd = open(DEVICE_NAME, O_RDWR);
   if (fd < 0) {
      printf("Failed to open the device for read/write\n");
      return -1;
   }
  
   pfd.fd = fd;
   pfd.events = (POLLOUT | POLLWRNORM );
      
   while( 1 ) 
   {
      puts("Starting poll...");
      
      ret = poll(&pfd, (unsigned long)1, 5000);   //wait for 5secs
      
      if( ret < 0 ) 
      {
         perror("poll");
         return -1;
      }

      if (ret == 0) {
         printf("poll timeout\n");
         continue;
      }
      
      if( ( pfd.revents & POLLOUT )  == POLLOUT )
      {
         char const *buff = TEST_STRING;
         ret = write(pfd.fd, buff, strlen(TEST_STRING));
         
         if (ret < 0) {
            printf("Failed to write the message %d, %d.\n", ret, errno);
         } else {
            printf("Written %d bytes\n", ret);
         }
         // reset file position
         if (lseek(fd, 0, SEEK_SET) < 0) {
            printf("Failed to seek the device.\n");
         }
      }
  }

   close(fd);

   return 0;
}