#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

const int RESULT_ERROR = -1;
const int RESULT_OK = 0;
#define true 1
#define false 0

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

	if (result > 0) {
		printf("read operation executed succesfully\n");
		printf("string read %s \n", data);
	}

	return result;
}

int check_result_less_than(int result, int expected_result,
			   const char *operation)
{
	if (result < expected_result) {
		printf("%s failed. Result %d, error:%s", operation, result,
		       strerror(errno));
		return false;
	}
	return true;
}

int main()
{
	int fd, result;

	fd = test_open("/dev/seqchardev_0");

	if (!check_result_less_than(fd, RESULT_OK, "open"))
		goto out;

	printf("File opened\n");

	result = test_write(fd, "TestMessage");

	if (!check_result_less_than(result, RESULT_OK, "write"))
		goto out;

	printf("write operation executed succesfully\n");

	result = lseek(fd, 0, SEEK_SET);
	if (!check_result_less_than(result, RESULT_OK, "lseek"))
		goto out;

	result = test_read(fd, 4);
	if (!check_result_less_than(result, RESULT_OK, "read"))
		goto out;

	result = test_read(fd, 7);
	if (!check_result_less_than(result, RESULT_OK, "read"))
		goto out;

	result = lseek(fd, 0, SEEK_SET);
	if (!check_result_less_than(result, RESULT_OK, "lseek"))
		goto out;

	result = test_read(fd, 11);
	if (!check_result_less_than(result, RESULT_OK, "read"))
		goto out;

out:
	if (close(fd)) {
		perror("close failed \n");
		return errno;
	}

	printf("file closed\n");

	return 0;
}
