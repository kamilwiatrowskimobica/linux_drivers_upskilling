#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

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

void *test_write()
{
	const char *message =
		"UTX[p/<AnJFUr/s0tf6}- / oyZm - E7DFt #$Zs.mmLW = > _g ? Kb02Gj";
	for (int i = 0; i < 10; i++) {
		int fd, result;
		fd = open("/dev/chardev_0", O_RDWR);

		for (int j = 0; j < 10; j++) {
			result = write(fd, message, strlen(message));

			if (result < 0) {
				printf("Write failed. Result %d, error:%s",
				       result, strerror(errno));
			}
			printf("wrote\n");
			if (lseek(fd, 0, SEEK_SET) < 0) {
				printf("lseek failed (write).\n");
			}
		}
		if (close(fd)) {
			perror("close failed \n");
		}
	}
	return NULL;
}

void *test_read()
{
	char data[30];

	for (int i = 0; i < 10; i++) {
		int fd, result;

		fd = open("/dev/chardev_0", O_RDWR);
		for (int j = 0; j < 10; j++) {
			result = read(fd, data, 30);

			if (result > 0) {
				printf("read operation executed succesfully\n");
				printf("string read %s \n", data);
			} else {
				printf(" read failed\n");
			}
			if (lseek(fd, 0, SEEK_SET) < 0) {
				printf("lseek failed (read).\n");
			}
		}
		if (close(fd)) {
			perror("close failed \n");
		}
	}
	return NULL;
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
	pthread_t read_th, write_th;

	int fd, result;
	pthread_create(&write_th, NULL, test_write, NULL);
	pthread_create(&read_th, NULL, test_read, NULL);

	pthread_join(write_th, NULL);
	pthread_join(read_th, NULL);

	return 0;
}
