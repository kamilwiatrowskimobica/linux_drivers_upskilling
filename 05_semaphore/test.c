#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>

#define BUFFER_LENGTH 256
#define DEV_FILE_PATH "/dev/chardevice0"
#define TEST_COUNT 1000

void* write_worker(void* data)
{
    const char* message = "1234567890poiuytrewqasdfghjkl;.,mnbvcxz<-=[]\'/.)(*&)";

    for (int i = 0; i < TEST_COUNT; i++)
    {
        int fd = open(DEV_FILE_PATH, O_WRONLY);
        if (fd < 0)
        {
            printf("Failed to open file\n");
            break;
        }

        for (int j = 0; j < TEST_COUNT; j++)
        {
            if (write(fd, message, strlen(message)) < 0)
            {
                printf("Failed to write message\n");
                break;
            }

            if (lseek(fd, 0, SEEK_SET) < 0)
            {
                printf("Failed to seek device\n");
                break;
            }
        }

        close(fd);
    }

    return NULL;
}

void* read_worker()
{
    char buffer[BUFFER_LENGTH] = { 0 };

    for (int i = 0; i < TEST_COUNT; i++)
    {
        int fd = open(DEV_FILE_PATH, O_RDONLY);
        if (fd < 0)
        {
            printf("Failed to open file\n");
            break;
        }

        for (int j = 0; j < TEST_COUNT; j++)
        {
            if (read(fd, buffer, BUFFER_LENGTH) < 0)
            {
                printf("Failed to read message\n");
                break;
            }

            if (lseek(fd, 0, SEEK_SET) < 0)
            {
                printf("Failed to seek device\n");
                break;
            }
        }

        close(fd);
    }

    return NULL;
}

int main()
{
    pthread_t read_thread, write_thread;

    pthread_create(&write_thread, NULL, write_worker, NULL);
    pthread_create(&read_thread, NULL, read_worker, NULL);

    pthread_join(write_thread, NULL);
    pthread_join(read_thread, NULL);

    return 0;
}
