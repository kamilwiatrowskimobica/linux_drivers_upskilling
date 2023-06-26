/**
 * @file   mysimplechardev_test.c
 * @author Andrzej Dziarnik
 * @date   23 06 2023
 * @version 0.1
 * @brief  A Linux user space program that communicates with the mysimplechardev.
*/
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 256
static char receive[BUFFER_LENGTH];

int main()
{
	int ret, fd;
	char stringToSend[BUFFER_LENGTH];
	printf("Starting device test code example...\n");
	fd = open("/dev/mysimplechardev", O_RDWR);
	if (fd < 0) {
		perror("Failed to open the device...");
		return errno;
	}
	printf("Type in a short string to send to the kernel module:\n");
	scanf("%[^\n]%*c", stringToSend);
	printf("Writing message to the device [%s].\n", stringToSend);
	ret = write(fd, stringToSend, strlen(stringToSend));
	if (ret < 0) {
		perror("Failed to write the message to the device.");
		return errno;
	}

	printf("Press ENTER to read back from the device...\n");
	getchar();

	printf("Reading from the device...\n");
	ret = read(fd, receive, BUFFER_LENGTH);
	if (ret < 0) {
		perror("Failed to read the message from the device.");
		return errno;
	}
	printf("The received message is: [%s]\n", receive);
	printf("End of the program\n");
	return 0;
}