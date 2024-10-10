#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>


/*
 * Ioctl definitions for testing purpose
 */

/* Use 'p' as magic number */
#define SCULL_IOC_MAGIC  'p'

#define SCULL_IOCRESET    _IO(SCULL_IOC_MAGIC, 0)

/*
 * S means "Set" through a ptr,
 * T means "Tell" directly with the argument value
 * G means "Get": reply by setting through a pointer
 * Q means "Query": response is on the return value
 * X means "eXchange": switch G and S atomically
 * H means "sHift": switch T and Q atomically
 */
#define SCULL_IOCSQUANTUM _IOW(SCULL_IOC_MAGIC,  1, int)
#define SCULL_IOCSQSET    _IOW(SCULL_IOC_MAGIC,  2, int)
#define SCULL_IOCTQUANTUM _IO(SCULL_IOC_MAGIC,   3)
#define SCULL_IOCTQSET    _IO(SCULL_IOC_MAGIC,   4)
#define SCULL_IOCGQUANTUM _IOR(SCULL_IOC_MAGIC,  5, int)
#define SCULL_IOCGQSET    _IOR(SCULL_IOC_MAGIC,  6, int)
#define SCULL_IOCQQUANTUM _IO(SCULL_IOC_MAGIC,   7)
#define SCULL_IOCQQSET    _IO(SCULL_IOC_MAGIC,   8)
#define SCULL_IOCXQUANTUM _IOWR(SCULL_IOC_MAGIC, 9, int)
#define SCULL_IOCXQSET    _IOWR(SCULL_IOC_MAGIC,10, int)
#define SCULL_IOCHQUANTUM _IO(SCULL_IOC_MAGIC,  11)
#define SCULL_IOCHQSET    _IO(SCULL_IOC_MAGIC,  12)

#define SCULL_IOC_MAXNR 12


#define BUFF_LEN 256
#define DEV_PATH_LEN_MAX 50

static char devicePath[DEV_PATH_LEN_MAX] = {"/dev/scull"};
static char buff[BUFF_LEN] = {0};

int main(int argc, char* argv[])
{
   int ret;
   int fd;
   int devNb;
   int quantum = 0;
   int qset = 0;
   char msgToSend[BUFF_LEN] = {0};

   printf("Type in device number node to test:");
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

#if (0)
   /* Without closing and opening device again the message received is empty. */
   close(fd);
   fd = open(devicePath, O_RDWR);
   if(0 > fd)
   {
      printf("Failed to open the device.\n");
      close(fd);
      return errno;
   }
#endif

   /* After adding lseek file pointer can be set to the beginning
    * of the file with use of lseek API instead of reopening the file.
    */
   ret = lseek(fd, 0, SEEK_SET);
   if (0 > ret)
   {
      printf("Failed to seek the device.\n");
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

   printf("Do you want to test llseek:");
   scanf("%[^\n]%*c", buff);

   if(0 == strcmp("y", buff))
   {
	   int offset;

	   printf("Enter offset:\n");
	   scanf("%[^\n]%*c", buff);

	   offset = atoi(buff);

	   ret = lseek(fd, offset, SEEK_SET);

	   printf("Reading from the device...\n");
	   ret = read(fd, buff, BUFF_LEN);
	   if(0 > ret)
	   {
	      printf("Failed to read the message from the device.\n");
	      close(fd);
	      return errno;
	   }
	   printf("The message received from device is: %s\n", buff);
   }

   printf("--------ioctl testing--------\n");
   ioctl(fd, SCULL_IOCGQUANTUM, &quantum);
   printf("Actual quantum max size: %d\n", quantum);
   qset = ioctl(fd, SCULL_IOCQQSET);
   printf("Actual qset max size: %d\n", qset);

   close(fd);
   printf("--------Test of the scull device ended--------\n");

   return 0;
}
