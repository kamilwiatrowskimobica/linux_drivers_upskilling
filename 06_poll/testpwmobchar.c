#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>

#define PWMOBDEV_IOC_MAGIC 'a'
#define WR_VALUE _IOW(PWMOBDEV_IOC_MAGIC, 1, char *)
#define RD_VALUE _IOR(PWMOBDEV_IOC_MAGIC, 2, char *)

int main()
{
	int fd, ret;
	char value[256] = { 0 };
	char number[256] = { 0 };
	struct pollfd pfd;

	printf("\nOpening Driver\n");
	fd = open("/dev/pwmobChar_0", O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file...\n");
		return 0;
	}

	pfd.fd = fd;
	pfd.events = ( POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM );

	printf("Starting poll...");
	
	ret = poll(&pfd, (unsigned long)1, 10000);
	
	if( ( pfd.revents & POLLOUT )  == POLLOUT )
	{
		printf("Enter the Value to send\n");
		scanf("%s", number);
		printf("Writing Value to Driver\n");
		write(pfd.fd, number, strlen(number));
	}

	if( ( pfd.revents & POLLIN )  == POLLIN )
	{
		printf("Reading Value from Driver\n");
		read(pfd.fd, value, sizeof(value));
		printf("Value is %s\n", value);
	}
	printf("Closing Driver\n");
	close(fd);
}