#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <unistd.h>

#define BUFFER_LENGTH 1048                   ///< The buffer length
static char receive_buf[BUFFER_LENGTH];      ///< The receive buffer from the LKM
static char stringToSend[BUFFER_LENGTH];     ///< The send buffer to LKM

int main(){

   int ret, fd, idata;
   strcpy(stringToSend, "test string");

   printf("Starting device test code...\n");

   printf(" opening device...\n");
   fd = open("/dev/rzechar", O_RDWR);             // Open the device with write access
   if (fd < 0){
      fprintf(stderr, "Failed to open the device...");
      return -1;
   }
   printf("device opened\n");

   printf(" Writing message to the device [%s].\n", stringToSend);
   if (lseek(fd, 0, SEEK_SET) < 0) {
      fprintf(stderr, " lseek failed.\n");
      close(fd);
      return -1;
   }
   ret = write(fd, stringToSend, strlen(stringToSend)); // Send the string to the LKM
   if (ret < 0){
      fprintf(stderr, "Failed to write the message to the device. \n");
      close(fd);
      return ret;
   }
   if (lseek(fd, 0, SEEK_SET) < 0) {
      fprintf(stderr, "lseek failed.\n");
      close(fd);
      return -1;
   }
   ret = read(fd, receive_buf, BUFFER_LENGTH);        // Read the response from the LKM
   if (ret < 0){
      perror("Failed to read the message from the device.");
      return errno;
   }
   printf("The received message is: [%s]\n", receive_buf);

   close(fd);
   printf("device closed\n");

   printf("End of the program\n");
   return 0;
}
