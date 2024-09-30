#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFF_LEN 256
static char receive[BUFF_LEN];

int main()
{
   int ret;
   int fd;
   char stringToSend[BUFF_LEN];

   printf("Starting device test code example...\n");

   fd = open("/dev/CharDevEx", O_RDWR);
   if(0 > fd)
   {
      perror("Failed to open the device\n");
      return errno;
   }

   printf("Type in a short string to send to the kerenel module:\n");
   scanf("%[^\n]%*c", stringToSend);

   printf("Writing message to the device [%s].\n", stringToSend);
   ret = write(fd, stringToSend, strlen(stringToSend));
   if(0 > ret)
   {
      perror("Failed to write the message to the device.");
      return errno;
   }

   printf("Press ENTER to read back from the device...\n");
   getchar();

   printf("Reading from the device...\n");
   ret = read(fd, receive, BUFF_LEN);
   if(0 > ret)
   {
      perror("Failed to read the message from the device");
      return errno;
   }

   printf("The received message is: [%s]\n", receive);
   printf("End of the program\n");

   return 0;
}
