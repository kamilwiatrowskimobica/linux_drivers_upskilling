#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>

#define BUFFER_LENGTH 256
static char receive[BUFFER_LENGTH];
static char send[BUFFER_LENGTH];

typedef int (*TEST_FUNC)(void);

#define SCD_0 '0'
#define SCD_1 '1'
#define SCD_2 '2'

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

    printf("TEST %s : %s \n", name, result);
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
            return errno;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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
            return errno;
        }

        ret = read(fd, receive, 1);

        if (ret != 0)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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
            return errno;
        }

        memset(send, i, BUFFER_LENGTH);

        ret = write(fd, send, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        ret = read(fd, receive, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        if (memcmp(receive, send, BUFFER_LENGTH) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return ret;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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
            return errno;
        }

        memset(send, i, BUFFER_LENGTH);

        ret = write(fd, send, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of written bytes: %s\n", get_device(i));
            return ret;
        }

        // Write one byte more to force buffer overflow
        ret = write(fd, send, 1);

        if (ret != -1)
        {
            printf("Written to full device: %s  (%d)\n", get_device(i), ret);
            return ret;
        }

        // clean up (device is closed after error)
        fd = open(get_device(i), O_RDWR);
        ret = read(fd, receive, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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
            return errno;
        }

        // write 3 words: Ala,ma,szarego
        ret = write(fd, buf_1, strlen(buf_1));

        if (ret != strlen(buf_1))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        ret = write(fd, buf_2, strlen(buf_2));

        if (ret != strlen(buf_2))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        ret = write(fd, buf_3, strlen(buf_3));

        if (ret != strlen(buf_3))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        // read back last(3rd) word: szarego
        ret = read(fd, receive, strlen(buf_3));

        if (ret != strlen(buf_3))
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        if (memcmp(receive, buf_3, strlen(buf_3)) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return ret;
        }

        // write 1 more word: kota
        ret = write(fd, buf_4, strlen(buf_4));

        if (ret != strlen(buf_4))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        // read back
        // read back last(3rd) word: szarego
        ret = read(fd, receive, strlen(expected));

        if (ret != strlen(expected))
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        if (memcmp(receive, expected, strlen(expected)) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return ret;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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
            return errno;
        }

        memset(send, i, BUFFER_LENGTH);

        ret = write(fd, send, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        // re-open device
        ret = close(fd);
        fd = open(get_device(i), O_RDWR);

        ret = read(fd, receive, BUFFER_LENGTH);

        if (ret != BUFFER_LENGTH)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        if (memcmp(receive, send, BUFFER_LENGTH) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return ret;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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
            return errno;
        }

        ret = write(fd, buf_1, strlen(buf_1));

        if (ret != strlen(buf_1))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        ret = lseek(fd, 3, SEEK_SET);

        if (ret != 3)
        {
            printf("Unable to set offset: %s \n", get_device(i));
            return ret;
        }

        ret = read(fd, receive, 3);

        if (ret != 3)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        if (memcmp(receive, "Ala", 3) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return ret;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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
            return errno;
        }

        ret = write(fd, buf_1, strlen(buf_1));

        if (ret != strlen(buf_1))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        ret = lseek(fd, -4, SEEK_CUR);

        if (ret != 5)
        {
            printf("Unable to set offset: %s %d \n", get_device(i), ret);
            return ret;
        }

        ret = read(fd, receive, 5);

        if (ret != 5)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        if (memcmp(receive, "Alama", 5) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return ret;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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
            return errno;
        }

        ret = write(fd, buf_1, strlen(buf_1));

        if (ret != strlen(buf_1))
        {
            printf("Invalid number of written bytes: %s \n", get_device(i));
            return ret;
        }

        ret = lseek(fd, -BUFFER_LENGTH + 3, SEEK_END);

        if (ret != 3)
        {
            printf("Unable to set offset: %s %d \n", get_device(i), ret);
            return ret;
        }

        ret = read(fd, receive, 3);

        if (ret != 3)
        {
            printf("Invalid number of read bytes: %s \n", get_device(i));
            return ret;
        }

        if (memcmp(receive, "Ala", 3) != 0)
        {
            printf("Invalid data read from device %s \n", get_device(i));
            return ret;
        }

        ret = close(fd);

        if (ret != 0)
        {
            printf("Failed to close the device: %s \n", get_device(i));
            return ret;
        }
    }

    return 0;
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

    return 0;
}