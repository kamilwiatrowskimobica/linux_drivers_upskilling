#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 256

int main()
{
	int ret, fd;
	char buffer[BUFFER_LENGTH];
	char receive[BUFFER_LENGTH];

	printf("Starting mob device test\n");
	// Open the device with read/write access
	fd = open("/dev/mob", O_RDWR);
	if (fd < 0) {
		perror("Failed to open the device");
		return errno;
	}
	printf("Give a string for module\n");
	// Read in a string (with spaces)
	scanf("%[^\n]%*c", buffer);
	printf("Writing msg to the device [%s].\n", buffer);
	// Send the string to the LKM
	ret = write(fd, buffer, strlen(buffer));
	if (ret < 0) {
		perror("Failed to write the msg to the device.");
		return errno;
	}

	printf("Press ENTER to read back msg from the device...\n");
	getchar();

	printf("Reading back from the device...\n");
	// Read the response from the LKM
	ret = read(fd, receive, BUFFER_LENGTH);
	if (ret < 0) {
		perror("Failed to read the msg from the device.");
		return errno;
	}
	printf("Received msg: [%s]\n", receive);
	printf("End of app\n");

	return 0;
}