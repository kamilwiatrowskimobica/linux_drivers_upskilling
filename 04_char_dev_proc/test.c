#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define BUFFER_LENGTH 10
#define DEV0_FILE_PATH "/dev/chardevice0"
#define DEV1_FILE_PATH "/dev/chardevice1"

static char receiveBuffer[BUFFER_LENGTH];

int ret, fd;
char stringToSend[BUFFER_LENGTH];

void openReadWrite(const char* filepath)
{
    fd = open(filepath, O_RDWR);

    if (fd < 0)
    {
        perror("Failed to open device\n");
        return errno;
    }

    printf("File %s opened\n", filepath);
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
}

int main()
{
    openReadWrite(DEV0_FILE_PATH);
    printf("\n");
    openReadWrite(DEV1_FILE_PATH);

    return 0;
}
