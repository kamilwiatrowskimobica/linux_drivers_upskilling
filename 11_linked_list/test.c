// each write stores node with number
// each read displays all nodes of list

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define BUFFER_LENGTH 10
#define DEV_FILE_PATH "/dev/chardevice0"
static char receiveBuffer[BUFFER_LENGTH];

int main()
{
    int ret, fd;
    char stringToSend[BUFFER_LENGTH];

    fd = open(DEV_FILE_PATH, O_RDWR);

    if (fd < 0)
    {
        perror("Failed to open device\n");
        return errno;
    }

    printf("File %s opened\n", DEV_FILE_PATH);
    printf("Type string to pass to driver\n");
    scanf("%[^\n]%*c", stringToSend);
    printf("Writing message '%s'\n", stringToSend);

    ret = write(fd, stringToSend, strlen(stringToSend));

    printf("Press Enter to read back from the device\n");
    getchar();

    if (ret < 0)
    {
        perror("Failed to write to the device\n");
        return errno;
    }

    lseek(fd, 0, SEEK_SET);
    printf("Reading from the device\n");
    ret = read(fd, receiveBuffer, BUFFER_LENGTH);

    if (ret < 0)
    {
        perror("Failed to read from the device\n");
        return errno;
    }

    printf("Message: '%s'\n", receiveBuffer);

    return 0;
}
