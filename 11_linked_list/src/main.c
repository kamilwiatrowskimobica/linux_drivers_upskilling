#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define BUFFER_LENGTH 256

typedef struct {
	char dev_name[32];
	char msg[BUFFER_LENGTH];
	unsigned it_count;
} thread_data_t;

void* writing_thread(void *arg)
{
	int ret, fd;
	if (arg == NULL) {
		printf("%s arg is NULL\n", __FUNCTION__);
		return NULL;
	}

	thread_data_t *data = (thread_data_t *)arg;
	printf("writing_thread: Opening device: %s\n", data->dev_name);

	for (int i=0; i<data->it_count; i++) {
		fd = open(data->dev_name, O_RDWR);
		if (fd < 0) {
			perror("writing_thread: Failed to open the device");
			break;
		}
		for (int j=0; j<data->it_count; j++) {
			ret = write(fd, data->msg, strlen(data->msg));
			if (ret < 0) {
				perror("writing_thread: Failed to write the msg to the device.");
				break;
			}
			// printf("[%i] Wrote [%d bytes] msg:    [%s]\n", (i+1)*(j+1), ret, data->msg);
		}
	}
	close(fd);
	return NULL;
}

void* reading_thread(void *arg)
{
	int ret, fd;
	char read_buff[BUFFER_LENGTH] = {0};
	if (arg == NULL) {
		printf("%s arg is NULL\n", __FUNCTION__);
		return NULL;
	}

	thread_data_t *data = (thread_data_t *)arg;
	printf("reading_thread: Opening device: %s\n", data->dev_name);
	// Open the device with read/write access

	for (int i=0; i<data->it_count; i++) {
		fd = open(data->dev_name, O_RDWR);
		if (fd < 0) {
			perror("reading_thread: Failed to open the device.");
			break;
		}
		for (int j=0; j<data->it_count; j++) {
			memset(read_buff, 0, sizeof(read_buff));
			ret = read(fd, read_buff, BUFFER_LENGTH);
			if (ret < 0) {
				perror("reading_thread: Failed to read the msg from the device.");
				break;
			}
			// printf("[%i] Received [%d bytes] msg: [%s]\n", (i+1)*(j+1), ret, read_buff);
		}
	}
	close(fd);
	return NULL;
}

int main(int argc, char *argv[])
{
	pthread_t read_thread_1, write_thread_1;
	thread_data_t data = {0};
	if (argc > 1) {
		snprintf(data.dev_name, sizeof(data.dev_name), "/dev/mob_%s", argv[1]);
	} else {
		snprintf(data.dev_name, sizeof(data.dev_name), "/dev/mob_0");
	}

	printf("Give a string for module\n");
	scanf("%[^\n]%*c", data.msg);
	printf("Give number of iteration\n");
	scanf("%u", &data.it_count);	

	pthread_create( &write_thread_1, NULL, writing_thread, (void*) &data);
	usleep(10000); // prevent reading empty buffers
	pthread_create( &read_thread_1, NULL, reading_thread, (void*) &data);

	pthread_join(write_thread_1, NULL);
    pthread_join(read_thread_1, NULL);

	printf("End of app\n");

	return 0;
}