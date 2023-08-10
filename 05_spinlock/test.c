#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include <pthread.h>

#define BUFFER_LENGTH 256
static char receive[BUFFER_LENGTH];
static char send[BUFFER_LENGTH];

typedef int (*TEST_FUNC)(void);

#define SCD_0 '0'
#define SCD_1 '1'
#define SCD_2 '2'

#define RACE_CONDITION_TEST_COUNT 20


#define TEST_FAIL -1;
#define TEST_OK 0;

/* Get devaice name from id (SCD_0, SCD_1, SCD_2). */
static char* get_device(char id)
{
    static char device[] = "/dev/scd_dev_X";

    if (id < SCD_0 || id > SCD_2)
    {
        return NULL;
    }

    device[strlen(device) - 1] = id;

    return device;
}

/* Auxiliary function to run test function. */
static void run_test(char* name, TEST_FUNC test_func)
{
    int ret = 0;
    char pass[] = "PASS";
    char fail[] = "FAIL";
    char *result = NULL;

    ret = test_func();
    if (ret == 0)
    {
        result = pass;
    }
    else
    {
        result = fail;
    }

    printf("TEST %-30s %s \n", name, result);
}

/* --- TEST FUNCTIONS --- */

int test_open_close()
{
    int ret, fd;

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

int test_read_empty()
{
    int ret, fd;

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        ret = read(fd, receive, 1);

        if (ret != 0)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

int test_write_max_read_max()
{
    int ret, fd;

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        memset(send, i, BUFFER_LENGTH);

        ret = write(fd, send, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = read(fd, receive, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        if (memcmp(receive, send, BUFFER_LENGTH) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

int test_write_overflow()
{
    int ret, fd;

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        memset(send, i, BUFFER_LENGTH);

        ret = write(fd, send, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of written bytes: %s\n", get_device(i));
            return TEST_FAIL;
        }

        // Write one byte more to force buffer overflow
        ret = write(fd, send, 1);

        if (ret != -1)
        {
            printf("Written to full device: %s  (%d)\n", get_device(i), ret);
            return TEST_FAIL;
        }

        // clean up (device is closed after error)
        fd = open(get_device(i), O_RDWR);
        ret = read(fd, receive, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

int test_write_read_parts()
{
    int ret, fd;

    char buf_1[] = "Ala";
    char buf_2[] = "ma";
    char buf_3[] = "szarego";
    char buf_4[] = "kota";
    char expected[] = "Alamakota";

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        // write 3 words: Ala,ma,szarego
        ret = write(fd, buf_1, strlen(buf_1));

        if (ret != strlen(buf_1))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = write(fd, buf_2, strlen(buf_2));

        if (ret != strlen(buf_2))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = write(fd, buf_3, strlen(buf_3));

        if (ret != strlen(buf_3))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        // read back last(3rd) word: szarego
        ret = read(fd, receive, strlen(buf_3));

        if (ret != strlen(buf_3))
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        if (memcmp(receive, buf_3, strlen(buf_3)) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return TEST_FAIL;
        }

        // write 1 more word: kota
        ret = write(fd, buf_4, strlen(buf_4));

        if (ret != strlen(buf_4))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        // read back
        // read back last(3rd) word: szarego
        ret = read(fd, receive, strlen(expected));

        if (ret != strlen(expected))
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        if (memcmp(receive, expected, strlen(expected)) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

int test_data_persists()
{
    int ret, fd;

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        memset(send, i, BUFFER_LENGTH);

        ret = write(fd, send, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        // re-open device
        ret = close(fd);
        fd = open(get_device(i), O_RDWR);

        ret = read(fd, receive, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        if (memcmp(receive, send, BUFFER_LENGTH) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

int test_seek_set()
{
    int ret, fd;
    char buf_1[] = "Alamakota";

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        ret = write(fd, buf_1, strlen(buf_1));

        if (ret != strlen(buf_1))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = lseek(fd, 3, SEEK_SET);

        if (ret != 3)
        {
            printf("Unable to set offset: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = read(fd, receive, 3);

        if (ret != 3)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        if (memcmp(receive, "Ala", 3) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

int test_seek_cur()
{
    int ret, fd;
    char buf_1[] = "Alamakota";

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        ret = write(fd, buf_1, strlen(buf_1));

        if (ret != strlen(buf_1))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = lseek(fd, -4, SEEK_CUR);

        if (ret != 5)
        {
            printf("Unable to set offset: %s %d \n", get_device(i), ret);
            return TEST_FAIL;
        }

        ret = read(fd, receive, 5);

        if (ret != 5)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        if (memcmp(receive, "Alama", 5) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

int test_seek_end()
{
    int ret, fd;
    char buf_1[] = "Alamakota";

    for (char i = SCD_0; i <= SCD_2; i++)
    {
        fd = open(get_device(i), O_RDWR);

        if (fd < 0)
        {
            printf("Failed to open the device: %s %d\n", get_device(i), fd);
            return TEST_FAIL;
        }

        ret = write(fd, buf_1, strlen(buf_1));

        if (ret != strlen(buf_1))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = lseek(fd, -BUFFER_LENGTH + 3, SEEK_END);

        if (ret != 3)
        {
            printf("Unable to set offset: %s %d \n", get_device(i), ret);
            return TEST_FAIL;
        }

        ret = read(fd, receive, 3);

        if (ret != 3)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return TEST_FAIL;
        }

        if (memcmp(receive, "Ala", 3) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return TEST_FAIL;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return TEST_FAIL;
        }
    }

    return TEST_OK;
}

// Aux function for test_write_race_condition
void *write_worker(void *data)
{
    int fd = open(get_device(SCD_0), O_WRONLY);
    if (fd < 0)
    {
        printf("Failed to open the device for write\n");
    }
    for (int j = 0; j < RACE_CONDITION_TEST_COUNT; ++j)
    {
        if (write(fd, (char*)data, strlen((char*)data)) < 0)
        {
            printf("Failed to write the message to the device.\n");
            break;
        }
    }
    close(fd);
    return NULL;
}

// Test fails (not always) if mutex is comented out from scd_dev_write
int test_write_race_condition()
{
    pthread_t write_th1, write_th2, write_th3;
    char buff[BUFFER_LENGTH] = {0};
    int ret = 0;
    const int expected_data_size = 3 * 3 * RACE_CONDITION_TEST_COUNT; // 3 write threads, each writes 20 times 3 bytes.

    for (int i = 0; i < 100; i++)
    {
        pthread_create(&write_th1, NULL, write_worker, "aaa");
        pthread_create(&write_th2, NULL, write_worker, "bbb");
        pthread_create(&write_th3, NULL, write_worker, "ccc");

        pthread_join(write_th1, NULL);
        pthread_join(write_th2, NULL);
        pthread_join(write_th3, NULL);

        int fd = open(get_device(SCD_0), O_RDWR);
        if ((ret = read(fd, buff, BUFFER_LENGTH)) < 0)
        {
            printf("Failed to read the data from the device.\n");
            break;
        }

        // If there was no failure while accessing device the
        // total number of written bytes should be 180 (3 * 3 * 20)
        if(ret != expected_data_size)
            return TEST_FAIL;
    }
    return TEST_OK;
}

// Aux function for test_read_race_condition
void *read_worker(void *data)
{
    char buff[BUFFER_LENGTH] = {0};

    int fd = open(get_device(SCD_0), O_RDWR);
    if (fd < 0)
    {
        printf("Failed to open the device for write.\n");
    }
    for (int j = 0; j < RACE_CONDITION_TEST_COUNT; ++j)
    {
        if (read(fd, buff, 3) <= 0)
        {
            printf("Failed to read data from the device.\n");
            break;
        }
    }
    close(fd);
    return NULL;
}

// Test fails (not always) if mutex is comented out from scd_dev_read
int test_read_race_condition()
{
    pthread_t read_th1, read_th2, read_th3;
    char buff[BUFFER_LENGTH] = {0};
    int ret = 0;
    const int expected_data_size = 3 * 3 * RACE_CONDITION_TEST_COUNT; // 3 read threads, each reads 20 times 3 bytes.

    memset(buff, 0xAA, sizeof(buff));

    int fd = open(get_device(SCD_0), O_RDWR);

    for (int i = 0; i < 100; i++)
    {
        if ((ret = write(fd, buff, expected_data_size)) < 0)
        {
            printf("Failed to write data to the device.\n");
            return TEST_FAIL;
        }

        pthread_create(&read_th1, NULL, read_worker, NULL);
        pthread_create(&read_th2, NULL, read_worker, NULL);
        pthread_create(&read_th3, NULL, read_worker, NULL);

        pthread_join(read_th1, NULL);
        pthread_join(read_th2, NULL);
        pthread_join(read_th3, NULL);

        if ((ret = read(fd, buff, BUFFER_LENGTH)) < 0)
        {
            printf("Failed to read the data from the device.\n");
            break;
        }

        // All data should be read back from the buffer by read threads.
        if(ret != 0)
            return TEST_FAIL;
    }
    return TEST_OK;
}

int main()
{
    run_test("open/close", test_open_close);
    run_test("read from empty device", test_read_empty);
    run_test("write max/read max", test_write_max_read_max);
    run_test("write to full device", test_write_overflow);
    run_test("write/read parts", test_write_read_parts);
    run_test("data persists after close", test_data_persists);
    run_test("seek(SEEK_SET)", test_seek_set);
    run_test("seek(SEEK_CUR)", test_seek_cur);
    run_test("seek(SEEK_END)", test_seek_end);
    run_test("write: race condition", test_write_race_condition);
    run_test("read: race condition", test_read_race_condition);

    return 0;
}