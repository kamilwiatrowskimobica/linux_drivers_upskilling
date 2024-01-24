#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include <sys/mman.h>

#define NPAGES 16
#define TEST_TYPE 2 //1 - read, 2 -write, read

void print_mem(unsigned char* addr)
{
    printf("%d", getpagesize());

    for (int i = 0; i < NPAGES * getpagesize(); i += getpagesize()){
    	printf("%p: 0x%x,\t %p: 0x%x,\t %p: 0x%x,\t %p: 0x%x\n", &addr[i], addr[i], &addr[i + 1], addr[i + 1], &addr[i + 2], addr[i + 2], &addr[i + 3], addr[i + 3]);
    }
}




int main(){

   int ret, fd;
   unsigned char* mmap_addr;
   unsigned char* write_buf;
   int len = NPAGES * getpagesize();

   printf("Starting device test code...\n");
   printf("opening device...\n");

   fd = open("/dev/rzechar", O_RDWR);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }
   printf("device opened\n");

   mmap_addr = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, 0);
   printf("mmap addr = %p\n", mmap_addr);

   if (mmap_addr == MAP_FAILED) return -1;
   
   if(1 == TEST_TYPE) print_mem(mmap_addr);
   else{
   
   	printf("write test\n");
    	write_buf = malloc(len);
    	if (!write_buf){
        	printf("error during memory allocation");
        	close(fd);
        	return -1;
   	}
   	memset(write_buf, 0, len);
    	for (int i = 0; i < NPAGES * getpagesize(); i += getpagesize()){
       	write_buf[i] = 0xA0;
        	write_buf[i + 1] = 0xB0;
        	write_buf[i + 2] = 0xC0;
        	write_buf[i + 3] = 0xD0;
	}
	if (!write(fd, write_buf, len)){
		perror("Failed to write the message to the device.");
		close(fd);
		return errno;
   	}
   	print_mem(mmap_addr);
   }
   
   close(fd);
   printf("device closed\n");
   printf("End of the program\n");
   return 0;
}

/*
printf("Type in a short string to send to the kernel module:\n");
   scanf("%[^\n]%*c", stringToSend);                // Read in a string (with spaces)
   printf("Writing message to the device [%s].\n", stringToSend);
   ret = write(fd, stringToSend, strlen(stringToSend)); // Send the string to the LKM
   if (ret < 0){
      perror("Failed to write the message to the device.");
      return errno;
   }

   close(fd);
   printf("device closed\n");

   printf("Press ENTER to read back from the device...\n");
   getchar();

   printf("Reading from the device...\n");
   fd = open("/dev/rzechar", O_RDWR);
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }
   printf("device opened\n");
   ret = read(fd, receive, BUFFER_LENGTH);        // Read the response from the LKM
   if (ret < 0){
      perror("Failed to read the message from the device.");
      return errno;
   }
   printf("The received message is: [%s]\n", receive);
   */
