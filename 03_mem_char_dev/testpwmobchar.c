#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define DATA_MAX 100

int main()
{
	int ret, fd;
	char buffer[DATA_MAX] = { 0 };
	char receive[DATA_MAX] = { 0 };
	char device[] = { "/dev/pwmobChar_0" };

	printf("Opening device: %s\n", device);
	fd = open(device, O_RDWR); //read write access
	if (fd < 0) {
		perror("Failed to open the device");
		return errno;
	}
	printf("Give a string for module\n");
	scanf("%[^\n]%*c", buffer);
	printf("Writing data to the device [%s].\n", buffer);
	ret = write(fd, buffer, strlen(buffer)); // Send string
	if (0 > ret) {
		perror("Failed to write the msg to the device.");
		close(fd);
		return errno;
	}

	printf("Press ENTER to read back msg from the device...\n");
	getchar();

	ret = lseek(fd, 0, SEEK_SET);
	if (0 > ret) {
		printf("Failed to seek the device.\n");
		close(fd);
		return errno;
	}

	printf("Reading back from the device...\n");

	ret = read(fd, receive, DATA_MAX); //read data
	if (0 > ret) {
		perror("Failed to read the msg from the device.");
		close(fd);
		return errno;
	}

	close(fd);
	printf("Received [%d bytes] msg: [%s]\n", ret, receive);
	printf("End of app\n");

	return 0;
}