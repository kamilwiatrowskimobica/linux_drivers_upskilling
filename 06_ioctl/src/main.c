#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include "mobi_ioctl.h"

#define BUFFER_LENGTH 256

int main(int argc, char *argv[])
{
	int ret, fd;
	size_t driver_msg_len = 0;
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

	mob_ioctl_data_t ioctl_mob = {
      .off = 0,
      .len = strlen(buffer),
      .data = buffer
   	};

	if (ioctl(fd, MOBI_MSG_SET, (mob_ioctl_data_t *) &ioctl_mob) < 0) {
		perror("ioctl failed for cmd MOBI_MSG_SET.");
		close(fd);
		return errno;
	}

	printf("MOBI_MSG_SET: wrote %ld bytes to driver\n", ioctl_mob.len);

	ioctl_mob.off = 0;
	ioctl_mob.len = 0;
	ioctl_mob.data = &ioctl_mob.len;

	if (ioctl(fd, MOBI_MSG_LENGTH, (mob_ioctl_data_t *) &ioctl_mob) < 0) {
		perror("ioctl failed for cmd MOBI_MSG_LENGTH.");
		close(fd);
		return errno;
	}

	printf("MOBI_MSG_LENGTH: %ld\n", ioctl_mob.len);

	ioctl_mob.data = receive;

	if (ioctl(fd, MOBI_MSG_GET, (mob_ioctl_data_t *) &ioctl_mob) < 0) {
		perror("ioctl failed for cmd MOBI_MSG_GET.");
		close(fd);
		return errno;
	}

	close(fd);
	printf("Received msg: [%s]\n", receive);
	printf("End of app\n");

	return 0;
}