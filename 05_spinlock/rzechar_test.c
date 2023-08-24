#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<pthread.h>

#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
#define TEST_COUNT 100
static char receive_th1[BUFFER_LENGTH];     ///< The receive buffer from the LKM
static char receive_th2[BUFFER_LENGTH];     ///< The receive buffer from the LKM
static char stringToSend[BUFFER_LENGTH];    ///< The send buffer to LKM
static char stringToSend2[BUFFER_LENGTH];    ///< The send buffer to LKM

struct Args {
   char* buff;
   char* name;
};

void* read_worker(void *arg){
   int fd,ret;
   struct Args* my_argPtr = (struct Args*)arg;
   char* buffer = (*my_argPtr).buff;
   char* name = (*my_argPtr).name;

   printf("%s: opening device...\n", name);
   fd = open("/dev/rzechar", O_RDONLY);             // Open the device with read access
   if (fd < 0){
      fprintf(stderr, "%s: Failed to open the device...\n", name);
      return NULL;
   }
   printf("%s: device opened\n", name);
   
   for (int i = 0; i < TEST_COUNT; ++i) {
      if (lseek(fd, 0, SEEK_SET) < 0) {
			fprintf(stderr, "%s: lseek failed.\n", name);
		}
      ret = read(fd, buffer, BUFFER_LENGTH);        // Read the response from the LKM
      if (ret < 0){
         fprintf(stderr, "%s: Failed to read the message from the device.\n", name);
         continue;
      }
      printf("%s: The received message is: [%s]\n", name, buffer);
      if (lseek(fd, 0, SEEK_SET) < 0) {
			fprintf(stderr, "%s: lseek failed.\n", name);
		}
   }
   close(fd);
   printf("%s: device closed\n", name);

   return NULL;

}

void* write_worker(void * arg){
   int ret, fd;
   struct Args* my_argPtr = (struct Args*)arg;
   char* buffer = (*my_argPtr).buff;
   char* name = (*my_argPtr).name;

   printf("%s: opening device...\n", name);
   fd = open("/dev/rzechar", O_WRONLY);             // Open the device with write access
   if (fd < 0){
      fprintf(stderr, "%s: Failed to open the device...", name);
      return NULL;
   }
   printf("%s: device opened\n", name);
   
   for (int i = 0; i < TEST_COUNT; ++i) {
      printf("%s: Writing message to the device [%s].\n", name, buffer);
      if (lseek(fd, 0, SEEK_SET) < 0) {
			fprintf(stderr, "%s: lseek failed.\n", name);
		}
      ret = write(fd, buffer, strlen(buffer)); // Send the string to the LKM
      if (ret < 0){
         fprintf(stderr, "%s: Failed to write the message to the device. \n", name);
         continue;
      }
      if (lseek(fd, 0, SEEK_SET) < 0) {
			fprintf(stderr, "%s: lseek failed.\n", name);
		}
   }
   close(fd);
   printf("%s: device closed\n", name);

   return NULL;
}


int main(){

   pthread_t read_th1, read_th2, write_th1, write_th2;
   struct Args *arg_read1, *arg_read2, *arg_write1, *arg_write2;
   strcpy(stringToSend, "test string");
   strcpy(stringToSend2, "another string");

   printf("Starting device test code...\n");

   arg_read1 = malloc(sizeof(struct Args));
   (*arg_read1).buff = receive_th1;
   (*arg_read1).name = "rt1";
   
   pthread_create(&read_th1, NULL, read_worker, (void*) arg_read1);

   arg_read2 = malloc(sizeof(struct Args));
   (*arg_read2).buff = receive_th2;
   (*arg_read2).name = "rt2";
   
   pthread_create(&read_th2, NULL, read_worker, (void*) arg_read2);
   
   arg_write1 = malloc(sizeof(struct Args));
   (*arg_write1).buff = stringToSend;
   (*arg_write1).name = "wt1";

   pthread_create(&write_th1, NULL, write_worker, (void*) arg_write1);

   arg_write2 = malloc(sizeof(struct Args));
   (*arg_write2).buff = stringToSend2;
   (*arg_write2).name = "wt2";

   pthread_create(&write_th2, NULL, write_worker, (void*) arg_write2);

   pthread_join(read_th1, NULL);
	pthread_join(write_th1, NULL);
   pthread_join(read_th2, NULL);
	pthread_join(write_th2, NULL);


   printf("End of the program\n");
   return 0;
}