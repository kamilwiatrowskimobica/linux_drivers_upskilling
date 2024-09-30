#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>


#define BUFF_LEN 256
#define DEV_PATH_LEN_MAX 50

static char devicePath[DEV_PATH_LEN_MAX] = {"/dev/scull"};
static char buff[BUFF_LEN] = {0};

int main(int argc, char* argv[])
{
   int ret;
   int fd;
   int devNb;
   char msgToSend[BUFF_LEN] = {0};

   printf("Type in device number node to test:\n");
   scanf("%[^\n]%*c", buff);

   devNb = atoi(buff);
   if(0 > devNb)
   {
      printf("Device node number can't be negative!\n");
      return -1;
   }

   strcat(devicePath, buff);

   fd = open(devicePath, O_RDWR);
   if(0 > fd)
   {
      printf("Failed to open the device nb:%d.\n", devNb);
      close(fd);
      return errno;
   }

   printf("Opened device %s.\n", devicePath);

   printf("Type in a message to send to the kerenel module:\n");
   scanf("%[^\n]%*c", msgToSend);

   printf("Writing message to the device [%s].\n", msgToSend);
   ret = write(fd, msgToSend, strlen(msgToSend));
   if(0 > ret)
   {
      printf("Failed to write message to the device.\n");
      close(fd);
      return errno;
   }

   /* Without closing and opening device again the message received is empty. */
   close(fd);
   fd = open(devicePath, O_RDWR);
   if(0 > fd)
   {
      printf("Failed to open the device.\n");
      close(fd);
      return errno;
   }

   printf("Press ENTER to read back from the device...\n");
   getchar();

   printf("Reading from the device...\n");
   ret = read(fd, buff, BUFF_LEN);
   if(0 > ret)
   {
      printf("Failed to read the message from the device.\n");
      close(fd);
      return errno;
   }

   printf("The message received from device is: %s\n", buff);
   close(fd);
   printf("Test of the scull device ended.\n");

   return 0;
}
