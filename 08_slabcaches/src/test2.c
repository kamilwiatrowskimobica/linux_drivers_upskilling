#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <poll.h>

#include "mob_ioctl.h"

#define BUFF_LEN (256u)
static char recv[BUFF_LEN];

void spinlock_test_exec(void);
void semaphore_test_exec(void);
void ioctl_test(void);
void poll_test_exec(void);
void cache_test(void);

void *spinlock_test_function(void *p_data);
void *semaphore_test_function(void *p_data);
void *poll_write_test_function(void *p_data);
void *poll_read_test_function(void *p_data);

struct task_data {
	int id;
	char device[32];
};

int main(void)
{
	printf("Starting...\n");
	//spinlock_test_exec();
	//semaphore_test_exec();

	//ioctl_test();
	//poll_test_exec();
	cache_test();
}

void spinlock_test_exec()
{
	printf("Spinlock test inc...\n");

	const char *testing_device_zero = "/dev/mobchar0";
	const char *testing_device_one = "/dev/mobchar1";

	struct task_data td0 = {
		.id = 0,
	};
	struct task_data td1 = {
		.id = 1,
	};

	pthread_t th0, th1;

	strncpy(td0.device, testing_device_zero, sizeof(td0.device));
	strncpy(td1.device, testing_device_one, sizeof(td1.device));

	pthread_create(&th0, NULL, semaphore_test_function, &td0);
	pthread_create(&th1, NULL, semaphore_test_function, &td1);

	pthread_join(th1, NULL);
	pthread_join(th0, NULL);

	printf("Test Over...\n");
}

void semaphore_test_exec(void)
{
	printf("Semaphore test inc...\n");

	const char *testing_device_zero = "/dev/mobchar0";
	const char *testing_device_one = "/dev/mobchar0";

	struct task_data td0 = {
		.id = 0,
	};
	struct task_data td1 = {
		.id = 1,
	};

	pthread_t th0, th1;

	strncpy(td0.device, testing_device_zero, sizeof(td0.device));
	strncpy(td1.device, testing_device_zero, sizeof(td1.device));

	pthread_create(&th0, NULL, semaphore_test_function, &td0);
	pthread_create(&th1, NULL, semaphore_test_function, &td1);

	pthread_join(th1, NULL);
	pthread_join(th0, NULL);

	printf("Test Over...\n");
}

void *spinlock_test_function(void *p_data)
{
	struct task_data *data = (struct task_data *)p_data;
	int fl;

	printf("Task%d rolling\n", data->id);

	for (int it = 0; it < 20; it++) {
		if (fl = open(data->device, O_RDWR) < 0) {
			printf("Failed to open device\n");
		}
		sleep(1);
		close(fl);
	}

	return NULL;
}

void *semaphore_test_function(void *p_data)
{
	struct task_data *data = (struct task_data *)p_data;
	int fl;
	char txt[256];
	int ret;
	const char *pattern1 = "Copy this";
	const char *pattern2 = "Paste that";

	printf("Task%d rolling\n", data->id);

	for (int it = 0; it < 100; it++) {
		memset(txt, 0, sizeof(txt));

		if ((fl = open(data->device, O_RDWR)) < 0) {
			printf("Failed to open device\n");
			continue;
		}

		if (0 == data->id) {
			memcpy(txt, pattern1, strlen(pattern1));
		} else {
			memcpy(txt, pattern2, strlen(pattern2));
		}

		ret = write(fl, txt, strlen(txt));
		memset(txt, 0, sizeof(txt));
		close(fl);

		fl = open(data->device, O_RDWR);

		read(fl, txt, sizeof(txt)); // Tu je nes
		if (ret < 0) {
			printf("Error reading file\n");
			close(fl);
			return NULL;
		}
		close(fl);

		if (memcmp(txt, pattern1, strlen(txt)) &&
		    memcmp(txt, pattern2, strlen(pattern2))) {
			printf("NOT THE SAME\n");
			printf("%s:%ld != %s:%ld \n", txt, strlen(txt),
			       pattern1, strlen(pattern1));
			printf("%s:%ld != %s:%ld \n", txt, strlen(txt),
			       pattern2, strlen(pattern2));
			break;
		}
	}

	printf("Read buffer: \"%s\" from Task%d\n", txt, data->id);

	return NULL;
}

void ioctl_test(void)
{
	int fl;
	int ret;

	const char *testing_device_zero = "/dev/mobchar0";

	printf("IOCTL test inc...\n");

	printf("Opening dev...\n");
	fl = open(testing_device_zero, O_RDWR);

	if (fl < 0)
	{
		printf("Err opening file...\n");
		return;
	}

	printf("Simple hello cmd\n");
	ioctl(fl, MOB_DEVICE_HELLO, NULL);
	

	ioctl(fl, MOB_DEVICE_WRITE, &ret);
	printf("Ioctl var = %d\n", ret);

	ioctl(fl, MOB_DEVICE_READ, &ret);
	printf("Current ioctl_var = %d\n", ret);
	

	close(fl);
}

void cache_test(void)
{
	int fl;
	int ret = 20;

	const char *testing_device_zero = "/dev/mobchar0";

	printf("CACHE test inc...\n");

	printf("Opening dev...\n");
	fl = open(testing_device_zero, O_RDWR);

	if (fl < 0)
	{
		printf("Err opening file...\n");
		return;
	}
	

	ioctl(fl, MOB_CACHE_WRITE, &ret);
	printf("Cache var = %d\n", ret);

	ioctl(fl, MOB_CACHE_READ, &ret);
	printf("Current ioctl_var = %d\n", ret);
	

	close(fl);
}

void poll_test_exec(void)
{
	printf("Poll test inc...\n");

	const char *testing_device_zero = "/dev/mobchar0";

	struct task_data td0 = {
		.id = 0,
	};
	struct task_data td1 = {
		.id = 1,
	};

	pthread_t th0, th1;

	strncpy(td0.device, testing_device_zero, sizeof(td0.device));
	strncpy(td1.device, testing_device_zero, sizeof(td1.device));

	pthread_create(&th0, NULL, poll_read_test_function, &td0);
	pthread_create(&th1, NULL, poll_write_test_function, &td1);

	pthread_join(th0, NULL);
	pthread_join(th1, NULL);
}

void *poll_write_test_function(void *p_data)
{
	int fd;
	struct task_data *p_td = p_data;
	const char* w_const = "Written data!";
	printf("In poll write thd\n");

	if ((fd = open(p_td->device, O_RDWR)) < 0)
	{
		printf("Failed to open device \n");
		pthread_exit(&p_td->id);
	}

	sleep(5);
	write(fd, w_const, strlen(w_const));
	
	close(fd);

	return NULL;
}

void *poll_read_test_function(void *p_data)
{
	int fd;
	int ret;
	char r_buf[20] = {0};
	struct pollfd pfd;
	struct task_data *p_td = p_data;
	printf("In poll read thd\n");

	if ((fd = open(p_td->device, O_RDWR)) < 0) 
	{
		printf("Failed to open device \n");
		pthread_exit(&p_td->id);
	}

	pfd.fd = fd;
	pfd.events = (POLLIN | POLLRDNORM | POLLOUT | POLLWRNORM);
	ret = poll(&pfd, (unsigned long)1, 10000);
	if (pfd.revents & POLLIN)
	{
		read(fd, r_buf, sizeof(r_buf));
	}	
	printf("Read item -- %s\n", r_buf);
	close(fd);

	return NULL;
}