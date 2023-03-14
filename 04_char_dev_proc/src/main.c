#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 256

int main(int argc, char *argv[])
{
	int ret, fd;
	char buffer[BUFFER_LENGTH];
	char receive[BUFFER_LENGTH] = {0};
	char dev_name[32] = {0};
	if (argc > 1) {
		snprintf(dev_name, sizeof(dev_name), "/dev/mob_%s", argv[1]);
	} else {
		snprintf(dev_name, sizeof(dev_name), "/dev/mob_0");
	}
	

	printf("Opening device: %s\n", dev_name);
	// Open the device with read/write access
	fd = open(dev_name, O_RDWR);
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
		close(fd);
		return errno;
	}

	printf("Press ENTER to read back msg from the device...\n");
	getchar();

	ret = lseek(fd, 0, SEEK_SET);
	if (ret < 0) {
		printf("Failed to seek the device.\n");
		close(fd);
		return errno;
	}

	printf("Reading back from the device...\n");
	// Read the response from the LKM
	ret = read(fd, receive, BUFFER_LENGTH);
	if (ret < 0) {
		perror("Failed to read the msg from the device.");
		close(fd);
		return errno;
	}

	close(fd);
	printf("Received [%d bytes] msg: [%s]\n", ret, receive);
	printf("End of app\n");

	return 0;
}