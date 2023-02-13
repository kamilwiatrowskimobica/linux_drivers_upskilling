#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>

#define BUFF_LEN (256u)
static char recv[BUFF_LEN];

int main()
{
    int ret, fd;
    char send[BUFF_LEN];

    printf("Starting the device test...\n");

    fd = open("/dev/hello", O_RDWR);
    // open err check
    if (fd < 0)
    {
        perror("Failed to open the device...");
        return errno;
    }

    printf("Type in a string to send to KM:\n");
    // read until \n
    scanf("%[^\n]%*c", send); 
    printf("Input string for device %s\n", send);

    // send string
    ret = write(fd, send, strlen(send));
    if (ret < 0)
    {
        perror("Failed to write string to dev...");
        return errno;
    }

    printf("Press ENTER to read back from dev\n");
    // hold char
    getchar();

    printf("Reading from device...\n");
    ret = read(fd, recv, BUFF_LEN);
    if (ret < 0)
    {
        perror("Failed to read the msg from dev..."); 
        return errno;
    }

    printf("Received msg: %s\n", recv);
    printf("END\n");

    return 0;


}