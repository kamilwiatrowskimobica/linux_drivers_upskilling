#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>


#define TEST_COUNT 3000
#define BUFF_LEN 256
#define DEV_PATH_LEN_MAX 50


static char devicePath[DEV_PATH_LEN_MAX] = {"/dev/scull"};
static char buff[BUFF_LEN] = {0};
char msgToSend[BUFF_LEN] = {0};
int devNb;


void *write_worker(void *data)
{
   for (int i = 0; i < TEST_COUNT; ++i)
   {
      int fd = open(devicePath, O_WRONLY);
      if (fd < 0)
      {
     	 printf("Failed to open the device nb:%d for write.\n", devNb);
         break;
      }

      printf("Opened device %s for write.\n", devicePath);

      for (int j = 0; j < TEST_COUNT; ++j)
      {
         if (write(fd, msgToSend, strlen(msgToSend)) < 0)
         {
            printf("Failed to write the message to the device nb:%d.\n", devNb);
            break;
         }
      }

      close(fd);
   }
   return NULL;
}

void *read_worker(void *data)
{
   for (int i = 0; i < TEST_COUNT; ++i)
   {
      int fd = open(devicePath, O_RDONLY);
      if (fd < 0)
      {
    	 printf("Failed to open the device nb:%d for read.\n", devNb);
         break;
      }

      printf("Opened device %s for read.\n", devicePath);

      for (int j = 0; j < TEST_COUNT; ++j)
      {
         if (read(fd, buff, BUFF_LEN) < 0)
         {
            printf("Failed to read the message from the device nb:%d.\n", devNb);
            break;
         }
      }

      close(fd);
   }

   return NULL;
}

int main(int argc, char* argv[])
{
   pthread_t read_th, write_th;

   printf("Type in device number node to test:\n");
   scanf("%[^\n]%*c", buff);

   devNb = atoi(buff);
   if(0 > devNb)
   {
      printf("Device node number can't be negative!\n");
      return -1;
   }

   strcat(devicePath, buff);

   printf("Type in a message to send to the kerenel module:\n");
   scanf("%[^\n]%*c", msgToSend);

   pthread_create(&write_th, NULL, write_worker, NULL);
   pthread_create(&read_th, NULL, read_worker, NULL);

   pthread_join(write_th, NULL);
   pthread_join(read_th, NULL);

   return 0;
}
