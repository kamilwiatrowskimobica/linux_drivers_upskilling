#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

const int RESULT_ERROR = -1;
const int RESULT_OK = 0;

int test_open(const char *device_name)
{
	int fd;
	fd = open(device_name, O_RDWR);

	return fd;
}

int test_write(int fd, const char *message)
{
	int result;
	result = write(fd, (void *)message, strlen(message));

	return result;
}

int test_read(int fd, size_t len)
{
	int result;
	char data[len];
	result = read(fd, (void *)data, len);

	if (result == RESULT_OK) {
		printf("read operation executed succesfully\n");
		printf("string read %s \n", data);
	}

	return result;
}

int main()
{
	int fd, result;

	if ((fd = test_open("/dev/memchardev")) < 0) {
		perror("open failed \n");
		return RESULT_ERROR;
	}
	printf("File opened\n");

	result = test_write(fd, "TestMessage");

	if (result != RESULT_OK) {
		printf("write failed %s\n", strerror(errno));
		return RESULT_ERROR;
	}
	printf("write operation executed succesfully\n");

	result = test_read(fd, 4);
	if (result != RESULT_OK) {
		printf("read failed %s\n", strerror(errno));
		return RESULT_ERROR;
	}

	result = test_read(fd, 11);
	if (result != RESULT_OK) {
		printf("read failed %s\n", strerror(errno));
		return RESULT_ERROR;
	}

	if (close(fd)) {
		perror("close failed \n");
		return RESULT_ERROR;
	}

	printf("file closed\n");

	printf("Test passed\n");

	return 0;
}
