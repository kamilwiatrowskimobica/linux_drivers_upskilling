#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>


#define TEST_COUNT 2000000
#define BUFF_LEN 256
#define DEV_PATH_LEN_MAX 50


static char devicePath[DEV_PATH_LEN_MAX] = {"/dev/scull"};
static char buff[BUFF_LEN] = {0};
int devNb;


void *open_close_worker(void *)
{
   for (int i = 0; i < TEST_COUNT; ++i) {
      int fd = open(devicePath, O_RDONLY);
      if (fd < 0) {
         printf("Failed to open the device for write\n");
         break;
      }
      close(fd);
   }
   return NULL;
}

int main(int argc, char* argv[])
{
   pthread_t test_th_1, test_th_2, test_th_3, test_th_4, test_th_5, test_th_6, test_th_7, test_th_8, test_th_9, test_th_10;

   printf("Type in device number node to test:\n");
   scanf("%[^\n]%*c", buff);

   devNb = atoi(buff);
   if(0 > devNb)
   {
      printf("Device node number can't be negative!\n");
      return -1;
   }

   strcat(devicePath, buff);

   pthread_create(&test_th_1, NULL, open_close_worker, NULL);
   pthread_create(&test_th_2, NULL, open_close_worker, NULL);
   pthread_create(&test_th_3, NULL, open_close_worker, NULL);
   pthread_create(&test_th_4, NULL, open_close_worker, NULL);
   pthread_create(&test_th_5, NULL, open_close_worker, NULL);
   pthread_create(&test_th_6, NULL, open_close_worker, NULL);
   pthread_create(&test_th_7, NULL, open_close_worker, NULL);
   pthread_create(&test_th_8, NULL, open_close_worker, NULL);
   pthread_create(&test_th_9, NULL, open_close_worker, NULL);
   pthread_create(&test_th_10, NULL, open_close_worker, NULL);

   pthread_join(test_th_1, NULL);
   pthread_join(test_th_2, NULL);
   pthread_join(test_th_3, NULL);
   pthread_join(test_th_4, NULL);
   pthread_join(test_th_5, NULL);
   pthread_join(test_th_6, NULL);
   pthread_join(test_th_7, NULL);
   pthread_join(test_th_8, NULL);
   pthread_join(test_th_9, NULL);
   pthread_join(test_th_10, NULL);

   return 0;
}
