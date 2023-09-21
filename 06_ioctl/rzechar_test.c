#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "rzechar_ioctl.h"

#define BUFFER_LENGTH 1048               ///< The buffer length
static char receive_buf[BUFFER_LENGTH];     ///< The receive buffer from the LKM
static char stringToSend[BUFFER_LENGTH];    ///< The send buffer to LKM

int main(){

   int ret, fd, idata;
   struct iocdata ioctldata = {
      .offset = 0,
      .size = BUFFER_LENGTH,
      .data = receive_buf
   };
   strcpy(stringToSend, "test string");

   printf("Starting device test code...\n");

   printf(" opening device...\n");
   fd = open("/dev/rzechar", O_RDWR);             // Open the device with write access
   if (fd < 0){
      fprintf(stderr, "Failed to open the device...");
      return -1;
   }
   printf("device opened\n");

   if (ioctl(fd, RZECHAR_IOC_HELLO) < 0) {
      printf("RZECHAR_IOC_HELLO failed, err=%d\n", errno);
      close(fd);
      return -1;
   }
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
   if (ioctl(fd, RZECHAR_IOC_READ, &ioctldata) < 0) {
      printf("RZECHAR_IOC_READ failed, err=%d\n", errno);
      close(fd);
      return -1;
   }
   printf("RZECHAR read: %s\n", ioctldata.data);

   close(fd);
   printf("device closed\n");

   printf("End of the program\n");
   return 0;
}