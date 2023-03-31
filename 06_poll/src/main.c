#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <pthread.h>
#include "mobi_ioctl.h"

#define BUFFER_LENGTH 256
#define NFDS	1
#define TIME_UNIT	1000 //1s
#define WAIT_TIME  (10*TIME_UNIT)

typedef struct {
	char dev_name[32];
	char msg[BUFFER_LENGTH];
	unsigned it_count;
	unsigned delay; //sec
} thread_data_t;

void* writing_thread(void *arg)
{
	int ret, fd;
	if (arg == NULL) {
		printf("%s arg is NULL\n", __FUNCTION__);
		return NULL;
	}

	thread_data_t *data = (thread_data_t *)arg;
	printf("[WThread]: Opening device: %s\n", data->dev_name);

	fd = open(data->dev_name, O_RDWR);
	if (fd < 0) {
		perror("[WThread]: Failed to open the device");
		return NULL;
	}

	for (int i=0; i<data->it_count; i++) {
		ret = write(fd, data->msg, strlen(data->msg));
		if (ret < 0) {
			perror("[WThread]: Failed to write the msg to the device.");
			break;
		}

		printf("[WThread][%i] Wrote [%d bytes] msg:    [%s]\n", (i+1), ret, data->msg);

		ret = lseek(fd, 0, SEEK_SET);
		if (ret < 0) {
			perror("[WThread]: Failed to seek the device.");
			break;
		}

		sleep(data->delay);
	}

	close(fd);
	printf("[WThread]: Closed device: %s\n", data->dev_name);
	return NULL;
}

int main(int argc, char *argv[])
{
	int ret, fd, count;
	size_t driver_msg_len = 0;
	char buffer[BUFFER_LENGTH];
	char receive[BUFFER_LENGTH] = {0};
	char dev_name[32] = {0};
	pthread_t write_thread = 0;
	thread_data_t data = {0};
	struct pollfd pfd;

	if (argc > 1) {
		snprintf(data.dev_name, sizeof(data.dev_name), "/dev/mob_%s", argv[1]);
	} else {
		snprintf(data.dev_name, sizeof(data.dev_name), "/dev/mob_0");
	}
	
	printf("Give a string for module\n");
	scanf("%[^\n]%*c", data.msg);
	printf("Give number of iteration\n");
	scanf("%u", &data.it_count);
	printf("Give dalay in seconds\n");
	scanf("%u", &data.delay);

	pthread_create( &write_thread, NULL, writing_thread, (void*) &data);

	printf("[Main] Opening device: %s\n", data.dev_name);
	// Open the device with read/write access
	fd = open(data.dev_name, O_RDWR);
	if (fd < 0) {
		perror("Failed to open the device");
		return errno;
	}

	pfd.events = POLLIN | POLLRDNORM;
	pfd.fd = fd;

	count = data.it_count;
	while(count) {
		ret = poll(&pfd, NFDS, 10 * data.delay * TIME_UNIT);

		// printf("poll returned:%d, revents:0x%X\n", ret, pfd.revents);

		if (ret < 0) {
			perror("poll error");
			break;
		}

		if (ret == 0) {
			// printf("polling Timeout\n");
			continue;
		}

		if((pfd.revents & POLLIN) || (pfd.revents & POLLRDNORM))
		{
            // printf("Reading device\n");

            ret = read(pfd.fd, receive, sizeof(receive));
            if (ret < 0)
			{
               perror("Failed reading message from device");
            }
			else if (ret == 0) //EoF
			{
				ret = lseek(pfd.fd, 0, SEEK_SET);
				if (ret < 0) {
					perror("Failed to seek the device.");
					break;
				}
			   continue;
            }
			else
			{
               printf("Received %d bytes: [%s]\n", ret, receive);
            }
        }
		else
		{
			continue;
		}
		

		count--;
	}

	close(fd);
	printf("[Main] Closed device: %s\n", data.dev_name);
	if (write_thread)
		pthread_join(write_thread, NULL);
	printf("End of app\n");

	return 0;
}