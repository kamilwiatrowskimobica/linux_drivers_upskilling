#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

void *read_worker()
{
	int fd, result;
	size_t len = 20;
	char data[len];

	if ((fd = open("/dev/chardev_0", O_RDWR)) < 0) {
		printf("ERROR: Failed to open device\n");
		return;
	}

	result = read(fd, data, len);
	if (result > 0) {
		printf("read operation executed succesfully\n");
		printf("string read %s \n", data);
	} else {
		printf("read failed, result :%d\n", result);
	}
}

void *write_worker()
{
	int fd, result;
	size_t len = 20;
	const char *message = "TestMessage";

	if ((fd = open("/dev/chardev_0", O_RDWR)) < 0) {
		printf("ERROR: Failed to open device\n");
		return;
	}

	result = write(fd, (void *)message, strlen(message));
	if (result > 0) {
		printf("write operation executed succesfully\n");
	} else {
		printf("write failed, result :%d\n", result);
	}
}

int main()
{
	pthread_t write_th, read_th;

	pthread_create(&read_th, NULL, read_worker, NULL);
	pthread_create(&write_th, NULL, write_worker, NULL);

	pthread_join(read_th, NULL);

	pthread_join(write_th, NULL);

	return 0;
}
