/**
 * @file   testsimplechar.c
 * @author Michal Kozikowski
 * @date   10 June 2024
 * @version 0.1
*/

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <stdbool.h>

#define BUFF_LEN 256
#define DEVICE_NAME "/dev/mk_mob_0"
#define MSG_1 "Hello from the other thread!\n"
#define MSG_2 "From the other thread, Hello!\n"
#define TEST_COUNT 3000

void *write_worker(void *data)
{
   const char *msg;


   int fd = open(DEVICE_NAME, O_WRONLY);
   if (fd < 0) {
      printf("Failed to open the device for write\n");
      return NULL;
   }
   
   for (int j = 0; j < TEST_COUNT; ++j) {
      msg = j % 2 == 0 ? MSG_1 : MSG_2;
      if (write(fd, msg, strlen(msg)) < 0) {
         printf("Failed to write the message to the device.\n");
         break;
      }
      if (lseek(fd, 0, SEEK_SET) < 0) {
         printf("Failed to seek the device for write.\n");
         break;
      }
   }
   
   close(fd);
   return NULL;
}

void *read_worker(void *data)
{
   char buff[BUFF_LEN] = {0};
   static bool test_passed = true;
   
   int fd = open(DEVICE_NAME, O_RDONLY);
   if (fd < 0) {
      printf("Failed to open the device for read\n");
      return NULL;
   }

   for (int j = 0; j < TEST_COUNT; ++j) {
         if (read(fd, buff, BUFF_LEN) < 0) {
            printf("Failed to read the message from the device.\n");
            break;
         }
         if (lseek(fd, 0, SEEK_SET) < 0) {
            printf("Failed to seek the device for read.\n");
            break;
         }
         if (   memcmp(buff, MSG_1, strlen(MSG_1)) 
             && memcmp(buff, MSG_2, strlen(MSG_2)))
         {
               test_passed = false;        
         }
         memset(buff, 0, sizeof(buff));
      }
   close(fd);

   if (test_passed)
   {
         printf("Test passed!\n");
   }
   else
   {
         printf("Test failed!\n");
   }

   return NULL;
}

int main()
{
   pthread_t read_th, write_th;

   pthread_create(&write_th, NULL, write_worker, NULL);
   pthread_create(&read_th, NULL, read_worker, NULL);

   pthread_join(write_th, NULL);
   pthread_join(read_th, NULL);

   return 0;
}