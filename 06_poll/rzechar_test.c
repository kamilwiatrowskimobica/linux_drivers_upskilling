#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "rzechar_ioctl.h"
#include <poll.h>
#include <assert.h>


#define BUFFER_LENGTH 1048
#define TEST_COUNT 10
static char receive_th1[BUFFER_LENGTH];     ///< The receive buffer from the LKM
static char receive_th2[BUFFER_LENGTH];     ///< The receive buffer from the LKM
static char stringToSend[BUFFER_LENGTH];    ///< The send buffer to LKM
static char stringToSend2[BUFFER_LENGTH];    ///< The send buffer to LKM

struct Args {
   char* buff;
   char* name;
};

void* poll_worker(void *arg){
   int fd, ret, n;
   struct pollfd pfd;
   struct Args* my_argPtr = (struct Args*)arg;
   char* buffer = (*my_argPtr).buff;
   char* name = (*my_argPtr).name;
   char suffix = 48;

   printf("%s: opening device...\n", name);
   fd = open("/dev/rzechar", O_RDWR | O_NONBLOCK);             // Open the device with write access
   if (fd < 0){
      fprintf(stderr, "%s: Failed to open the device...", name);
      return NULL;
   }
   printf("%s: device opened\n", name);

   pfd.fd = fd;
   pfd.events = ( POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM );

   for (int i = 0; i < TEST_COUNT; ++i){
      puts("Starting poll...");
    
      ret = poll(&pfd, (unsigned long)1, 5000);   //wait for 5secs
      
      if( ret < 0 ) 
      {
         perror("poll");
         assert(0);
      }
      if (ret == 0) {
         printf("poll timeout\n");
         continue;
      }
    
    if( ( pfd.revents & POLLIN )  == POLLIN )
    {
      puts("pollin...");
      if (lseek(fd, 0, SEEK_SET) < 0) {
         fprintf(stderr, " lseek failed.\n");
      }
      ret = read(pfd.fd, buffer, BUFFER_LENGTH);
      if (ret < 0){
         fprintf(stderr, "%s: Failed to read the message from the device.\n", name);
         continue;
      }
      printf("POLLIN : %s\n", buffer);
      strcpy(stringToSend2, buffer);
      stringToSend2[0] = suffix++;

    }
    
    if( ( pfd.revents & POLLOUT )  == POLLOUT )
    {
      puts("pollout...");
      strcpy( buffer, "from user");
      if (lseek(fd, 0, SEEK_SET) < 0) {
         fprintf(stderr, " lseek failed.\n");
      }
      write(pfd.fd, stringToSend2, BUFFER_LENGTH);
      if (ret < 0){
         fprintf(stderr, "%s: Failed to write the message to the device. \n", name);
         continue;
      }
      printf("POLLOUT : %s\n", stringToSend2);
    }
  }

   close(fd);
   printf("%s: device closed\n", name);
}


int main(){
   int ret, fd0;
   pthread_t poll_th;
   struct Args *arg_poll;
   strcpy(stringToSend, "test string from user");

   printf("Starting device test code...\n");

   printf(" opening device...\n");
   fd0 = open("/dev/rzechar", O_RDWR);             // Open the device with write access
   if (fd0 < 0){
      fprintf(stderr, "Failed to open the device...");
      return -1;
   }
   printf("device opened\n");
   printf(" Writing message to the device [%s].\n", stringToSend);
   if (lseek(fd0, 0, SEEK_SET) < 0) {
      fprintf(stderr, " lseek failed.\n");
      close(fd0);
      return -1;
   }
   ret = write(fd0, stringToSend, strlen(stringToSend)); // Send the string to the LKM
   if (ret < 0){
      fprintf(stderr, "Failed to write the message to the device. \n");
      close(fd0);
      return ret;
   }

   close(fd0);
   printf("device closed\n");

   arg_poll = malloc(sizeof(struct Args));
   (*arg_poll).buff = receive_th2;
   (*arg_poll).name = "poll";

   pthread_create(&poll_th, NULL, poll_worker, (void*) arg_poll); 

   pthread_join(poll_th, NULL);


   printf("End of the program\n");
   return 0;
}