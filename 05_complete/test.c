#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// spinlock configuration
#define SPINLOCK_TIMES 1250000

// mutex configuration
#define MUTEX_TIMES 500
#define PATTERN_SIZE 1024
char pattern_1_value = 0x5a;
char pattern_2_value = 0xa5;
char pattern_1_table [PATTERN_SIZE];
char pattern_2_table [PATTERN_SIZE];

struct thread_params {
	int id;
	char device[32];
};

void *spinlock_worker(void *data);
void *mutex_worker(void *data);
void *wait_for_completion_reader(void *data);
void *wait_for_completion_writer(void *data);

void spinlock_tests(void);
void mutex_tests(void);
void wait_for_completion_tests(void);

int main(void)
{	
	printf("Testing spinlock:\n");
	spinlock_tests();
	
	printf("\n\nTesting mutex:\n");
	mutex_tests();
	
	printf("\n\nTesting wait for completion mode:\n");
	wait_for_completion_tests();
	
	printf("\n\nExiting from main program\n");
	return 0;
}

void spinlock_tests(void)
{
	const char *default_device = "/dev/woabchar0";
	const char *spinlock_device1 = "/dev/woabchar1";
	const char *spinlock_device2 = "/dev/woabchar2";
	const char *proc_file = "/proc/woabchar";
	const char *ptr[] = { default_device, spinlock_device1, spinlock_device2 };
	const int size = sizeof(ptr) / sizeof(const char*);
	
	struct timeval tval_before, tval_after, tval_result;
	struct thread_params th1p, th2p;
	pthread_t th1, th2;
	int i, device, opens_before[size], opens;

	FILE* proc;

	th1p.id = 1;
	th2p.id = 2;

	proc = fopen(proc_file, "r");
	while (fscanf(proc, "%*s %d %*s %d %*s", &device, &opens) == 2) {
		opens_before[device] = opens;
	}
	fclose(proc);

	for(i = 0; i < size; i++) {
		printf("Testing %s device for spinlock operations\n", ptr[i]);
		strcpy(th1p.device, ptr[i]);
		strcpy(th2p.device, ptr[i]);
		
		pthread_create(&th1, NULL, spinlock_worker, &th1p);
		pthread_create(&th2, NULL, spinlock_worker, &th2p);
		
		/* measure time once threads are created */
		gettimeofday(&tval_before, NULL);
		
		(void) pthread_join(th1, NULL);
		(void) pthread_join(th2, NULL);
		
		/* measure time once threads are done */
		gettimeofday(&tval_after, NULL);
		
		timersub(&tval_after, &tval_before, &tval_result);
		
		printf("\tTime elapsed: %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
	}
	
	proc = fopen(proc_file, "r");
	
	printf("Results:\n");
	i = 0;
	while (fscanf(proc, "%*s %d %*s %d %*s", &device, &opens) == 2) {
		printf("\tDevice %d opened %d times, expected %d\n", device, opens - opens_before[i], SPINLOCK_TIMES * 2);
		i++;
	}
	
	fclose(proc);
}

void mutex_tests(void)
{
	memset(pattern_1_table, pattern_1_value, sizeof(pattern_1_table));
	memset(pattern_2_table, pattern_2_value, sizeof(pattern_2_table));
	
	const char *default_device = "/dev/woabchar0";
	const char *mutex_device = "/dev/woabchar2";

	const char *ptr[] = { default_device, mutex_device };
	
	struct timeval tval_before, tval_after, tval_result;
	struct thread_params th1p, th2p;
	pthread_t th1, th2;
	int i;

	th1p.id = 1;
	th2p.id = 2;

	for(i = 0; i < 2; i++) {
		printf("Testing %s device for mutex operations\n", ptr[i]);
		strcpy(th1p.device, ptr[i]);
		strcpy(th2p.device, ptr[i]);
		
		pthread_create(&th1, NULL, mutex_worker, &th1p);
		pthread_create(&th2, NULL, mutex_worker, &th2p);
		
		/* measure time once threads are created */
		gettimeofday(&tval_before, NULL);
		
		(void) pthread_join(th1, NULL);
		(void) pthread_join(th2, NULL);
		
		/* measure time once threads are done */
		gettimeofday(&tval_after, NULL);
		
		timersub(&tval_after, &tval_before, &tval_result);
		
		printf("\tTime elapsed: %ld.%06ld\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
	}
}

void wait_for_completion_tests(void)
{
	const char *completion_device = "/dev/woabchar1";
		
	struct timeval tval_before, tval_after, tval_result;
	struct thread_params th1p, th2p;
	pthread_t th1, th2;

	th1p.id = 1;
	th2p.id = 2;


	printf("Testing %s device for wait for completion mode [MAIN]\n", completion_device);
	strcpy(th1p.device, completion_device);
	strcpy(th2p.device, completion_device);
		
	pthread_create(&th1, NULL, wait_for_completion_reader, &th1p);
	pthread_create(&th2, NULL, wait_for_completion_writer, &th2p);
		
	/* measure time once threads are created */
	gettimeofday(&tval_before, NULL);
		
	(void) pthread_join(th1, NULL);
	(void) pthread_join(th2, NULL);
		
	/* measure time once threads are done */
	gettimeofday(&tval_after, NULL);
		
	timersub(&tval_after, &tval_before, &tval_result);
		
	printf("\tTime elapsed: %ld.%06ld [MAIN]\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
}

void *spinlock_worker(void *data)
{
	struct thread_params *ptr = data;
	int fd;
	for (int i = 0; i < SPINLOCK_TIMES; i++)
	{
		if ((fd = open(ptr->device, O_RDWR)) < 0 ) {
			printf("ERROR: Failed to open device, resulst are affected\n");
			continue;
		}
		close(fd);
	}

	return NULL;
}

void *mutex_worker(void *data)
{
	int i, fd, err = 0;
	struct thread_params *ptr = data;
	char bytes[PATTERN_SIZE];
	
	for (int i = 0; i < SPINLOCK_TIMES; i++)
	{
		if ((fd = open(ptr->device, O_RDWR)) < 0 ) {
			printf("ERROR: Failed to open device, resulst are affected\n");
			continue;
		}
		if(1 == ptr->id) {
			memset(bytes, pattern_1_value, sizeof(bytes));
		} else {
			memset(bytes, pattern_2_value, sizeof(bytes));
		}
		write(fd,  bytes, sizeof(bytes));
		memset(bytes, 0, sizeof(bytes));
		close(fd);
		
		if ((fd = open(ptr->device, O_RDWR)) < 0 ) {
			printf("ERROR: Failed to open device, resulst are affected\n");
			continue;
		}
		read(fd, bytes, sizeof(bytes));
		close(fd);
		
		if((0 != memcmp(bytes, pattern_1_table, sizeof(bytes))) &&
			(0 != memcmp(bytes, pattern_2_table, sizeof(bytes)))) {
			err++;
			int j;
		}
	}
	
	printf("Thread %d pattern does not match %d times\n", ptr->id, err);
}

void *wait_for_completion_reader(void *data)
{
	struct thread_params *ptr = data;
	int fd, ret;
	char bytes[16] = {0};
	
	printf("Testing %s device for wait for completions operations [READ THREAD %d]\n", ptr->device, ptr->id);
	if ((fd = open(ptr->device, O_RDONLY)) < 0 ) {
		printf("ERROR: Failed to open device, resulst are affected [READ THREAD %d]\n", ptr->id);
		return NULL;
	}
	
	printf("\tDevice %s opened for reading [READ THREAD %d]\n", ptr->device, ptr->id);
	
	ret = read(fd, bytes, sizeof(bytes));
	printf("\tThread %d succesfully read %d bytes from device %s [READ THREAD %d]\n", ptr->id, ret, ptr->device, ptr->id);
	close(fd);
	
	return NULL;
}

void *wait_for_completion_writer(void *data)
{
	struct thread_params *ptr = data;
	int fd, ret;
	char bytes[16] = {0};
	
	printf("Testing %s device for wait for completions operations [WRITE THREAD %d]\n", ptr->device, ptr->id);
	if ((fd = open(ptr->device, O_WRONLY)) < 0 ) {
		printf("ERROR: Failed to open device, resulst are affected [WRITE THREAD %d]\n", ptr->id);
		return NULL;
	}
	
	printf("\tDevice %s opened for writting [WRITE THREAD %d]\n", ptr->device, ptr->id);
	printf("\tIntentional sleep before writing for 3s [WRITE THREAD %d]\n", ptr->id);
	sleep(3);
	
	ret = write(fd, bytes, sizeof(bytes));
	printf("\tThread %d succesfully wrote %d bytes to %s\n", ptr->id, ret, ptr->device);
	close(fd);
}