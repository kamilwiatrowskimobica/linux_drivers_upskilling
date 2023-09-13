#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include "twdrv_main.h"

#define BUFFER_LENGTH 20

static char stringReceived[MAX_DEVICE_SIZE];


void writedev(int dev_number, char *stringToSend) {
   int fd, ret;
   char stringDevice[BUFFER_LENGTH];
   sprintf(stringDevice, "/dev/%s%d", MODULE_NAME, dev_number);
   printf("Starting device test code example...\n");
   fd = open(stringDevice, O_RDWR); 
   if (fd < 0){
      perror("Failed to open the device...");
      return;
   }
   printf("Writing message to the device [%s]: %s.\n", stringDevice, stringToSend);
   ret = write(fd, stringToSend, strlen(stringToSend)); 
   if (ret < 0){
      perror("Failed to write the message to the device.");
      return;
   }
   return;
}

void readdev(int dev_number) {
   int ret, fd;
   char stringDevice[BUFFER_LENGTH];
   sprintf(stringDevice, "/dev/%s%d", MODULE_NAME, dev_number);
   fd = open(stringDevice, O_RDWR); 
   ret = read(fd, stringReceived, MAX_DEVICE_SIZE);
   if (ret < 0){
      perror("Failed to read the message from the device.");
      return;
   }
   printf("The received message for device %s is: \n%s\n", stringDevice, stringReceived);
   return;
}

int main(){
   int ret, fd;
   char stringToSend[MAX_DEVICE_SIZE];
   char *received;
   for (int i = 0; i < NUM_DEVICES; i++) {
      memset(stringToSend, 0, sizeof(*stringToSend));
      sprintf(stringToSend, "DEVICE %d DATA", i);
      writedev(i, stringToSend);
   }

   for (int i = 0; i < NUM_DEVICES; i++) {
      readdev(i);
   }
   return 0;
}
