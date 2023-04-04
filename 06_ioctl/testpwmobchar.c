#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define PWMOBDEV_IOC_MAGIC 'a'
#define WR_VALUE _IOW(PWMOBDEV_IOC_MAGIC, 1, char *)
#define RD_VALUE _IOR(PWMOBDEV_IOC_MAGIC, 2, char *)

int main()
{
	int fd;
	char value[256] = { 0 };
	char number[256] = { 0 };

	printf("\nOpening Driver\n");
	fd = open("/dev/pwmobChar_0", O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file...\n");
		return 0;
	}

	printf("Enter the Value to send\n");
	scanf("%s", number);
	printf("Writing Value to Driver\n");
	ioctl(fd, WR_VALUE, (char *)number);

	printf("Reading Value from Driver\n");
	ioctl(fd, RD_VALUE, (char *)value);
	printf("Value is %s\n", value);

	printf("Closing Driver\n");
	close(fd);
}