#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>

#define PATTERN_SIZE (1024 * 16)
char pattern_1_value = 'a';
char pattern_2_value = 'b';
char pattern_1_table[PATTERN_SIZE];
char pattern_2_table[PATTERN_SIZE];
static int error = 0;

void *write_read_worker(void *arg)
{
	char data[PATTERN_SIZE];
	char response[PATTERN_SIZE];

	int *id = arg;
	if (*id == 1)
		memset(data, pattern_1_value, sizeof(data));
	else
		memset(data, pattern_2_value, sizeof(data));

	for (int i = 0; i < 100000; i++) {
		int fd, result;
		if ((fd = open("/dev/chardev_0", O_RDWR)) < 0) {
			printf("ERROR: Failed to open device\n");
			continue;
		}
		result = write(fd, data, strlen(data));
		if (result < 0) {
			printf("Write failed. Result %d, error:%s, strlen %d",
			       result, strerror(errno), strlen(data));
		}

		if (lseek(fd, 0, SEEK_SET) < 0) {
			printf("lseek failed (write).\n");
		}
		memset(response, 0, sizeof(response));

		result = read(fd, response, PATTERN_SIZE);
		if (result <= 0) {
			printf(" read failed %d\n", result);
		}
		close(fd);

		if ((0 !=
		     memcmp(response, pattern_1_table, sizeof(response))) &&
		    (0 !=
		     memcmp(response, pattern_2_table, sizeof(response)))) {
			error++;
		}
	}
}

int main()
{
	pthread_t write_th1, write_th2, write_th3;

	int id;
	memset(pattern_1_table, pattern_1_value, sizeof(pattern_1_table));
	memset(pattern_2_table, pattern_2_value, sizeof(pattern_2_table));

	id = 1;
	pthread_create(&write_th1, NULL, write_read_worker, &id);
	id = 2;

	pthread_create(&write_th2, NULL, write_read_worker, &id);

	pthread_join(write_th1, NULL);

	pthread_join(write_th2, NULL);

	printf("Errors: %d\n", error);

	return 0;
}
