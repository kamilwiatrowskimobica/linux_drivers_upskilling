#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

#define PATTERN_SIZE (1024*16)
char pattern_1_value = 0x5a;
char pattern_2_value = 0xa5;
char pattern_1_table [PATTERN_SIZE];
char pattern_2_table [PATTERN_SIZE];

struct thread_params {
	int id;
	char device[32];
};

void *reader(void *data);
void *writer(void *data);

void completion_tests(void);

int main(void)
{	

	printf("\nStart test\n");
	completion_tests();
	
	printf("\nExit test\n");
	return 0;
}

void completion_tests(void)
{
	const char *completion_device = "/dev/mobchar0";
		
	struct timeval tval_before, tval_after, tval_result;
	struct thread_params th1p, th2p;
	pthread_t th1, th2;

	th1p.id = 1;
	th2p.id = 2;
	strcpy(th1p.device, completion_device);
	strcpy(th2p.device, completion_device);
		
	pthread_create(&th1, NULL, reader, &th1p);
	pthread_create(&th2, NULL, writer, &th2p);

	gettimeofday(&tval_before, NULL);
	(void) pthread_join(th1, NULL);
	(void) pthread_join(th2, NULL);
	gettimeofday(&tval_after, NULL);
	
	timersub(&tval_after, &tval_before, &tval_result);
	
	printf("\tTime elapsed: %ld.%06ld [MAIN]\n", (long int)tval_result.tv_sec, (long int)tval_result.tv_usec);
}

void *reader(void *data)
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

void *writer(void *data)
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