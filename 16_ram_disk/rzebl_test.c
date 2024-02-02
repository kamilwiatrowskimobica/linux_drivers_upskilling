#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include <sys/mman.h>

#define NR_SECTORS 128
#define SECTOR_SIZE 512
#define TEST_TYPE 1 //1 - read, 2 -write, read

void testwrite(int fd, char val){
	int i,j;
	char buffer[SECTOR_SIZE];	
	printf("%d", getpagesize());

	for (i = 0; i < NR_SECTORS; i++){
		for (j = 0; j < sizeof(buffer) / sizeof(buffer[0]); j++) buffer[j] = val+i;
		write(fd, buffer, sizeof(buffer));
		printf("write sector %d ... \n", i);
	}
}

void testcheck(int fd, char val){
	int i,j;
	char buffer[SECTOR_SIZE];
	char cmpbuffer[SECTOR_SIZE];

	for (i = 0; i < NR_SECTORS; i++){
		for (j = 0; j < sizeof(cmpbuffer) / sizeof(cmpbuffer[0]); j++) cmpbuffer[j] = val+i;
		read(fd, buffer, sizeof(buffer));
		printf("read sector %d ... ", i);
		
		if (memcmp(cmpbuffer, buffer, sizeof(buffer)) == 0)
			printf("passed\n");
		else
			printf("failed\n");
	}
}

int main(){

   int ret, fd;

   printf("Starting device test code...\n");
   printf("opening device...\n");

   fd = open("/dev/rzeblk", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }
   printf("device opened\n");

   testwrite(fd, 'a');
   
   close(fd);
   printf("device closed\n");
   
   fd = open("/dev/rzeblk", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }
   printf("device opened\n");

   testcheck(fd, 'a');
   
   close(fd);
   printf("device closed\n");
   
   printf("End of the program\n");
   return 0;
}

