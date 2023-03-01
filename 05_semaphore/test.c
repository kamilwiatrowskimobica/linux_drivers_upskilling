#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

// mutex configuration
#define MUTEX_TIMES 100
#define PATTERN_SIZE (1024*16)
char pattern_1_value = 0x5a;
char pattern_2_value = 0xa5;
char pattern_1_table [PATTERN_SIZE];
char pattern_2_table [PATTERN_SIZE];

struct thread_params {
	int id;
	char device[32];
};

void *mutex_worker(void *data);
void mutex_tests(void);

int main(void)
{
	printf("\n\nTesting mutex:\n");
	mutex_tests();
	
	printf("\n\nExiting from main program\n");
	return 0;
}

void mutex_tests(void)
{
	memset(pattern_1_table, pattern_1_value, sizeof(pattern_1_table));
	memset(pattern_2_table, pattern_2_value, sizeof(pattern_2_table));
	
	const char *default_device = "/dev/mobchar0";

	const char *ptr[] = { default_device, default_device };
	
	struct timeval tval_before, tval_after, tval_result;
	struct thread_params th1p, th2p;
	pthread_t th1, th2;
	int i;

	th1p.id = 1;
	th2p.id = 2;

	for(i = 0; i < 2; i++) {
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

void *mutex_worker(void *data)
{
	int i, fd, err = 0;
	struct thread_params *ptr = data;
	char bytes[PATTERN_SIZE];
	
	for (int i = 0; i < MUTEX_TIMES; i++)
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