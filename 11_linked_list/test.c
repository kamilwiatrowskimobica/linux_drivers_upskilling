#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>

#define MESSAGE_LEN 256

int main(){

    int ret;
    int fd;
    char buf[MESSAGE_LEN];
    
    fd = open("/dev/pm_char_device", O_RDWR);
        
    if(fd < 0){
        perror("Opening for read error\n");
        return errno;
    } 

    memset(buf, 0, MESSAGE_LEN);
    sprintf(buf, "test1");
    ret = write(fd, buf, strlen(buf));
    
    memset(buf, 0, MESSAGE_LEN);
    sprintf(buf, "test2");
    ret = write(fd, buf, strlen(buf));

    memset(buf, 0, MESSAGE_LEN);
    sprintf(buf, "test3");
    ret = write(fd, buf, strlen(buf));

    memset(buf, 0, MESSAGE_LEN);
    ret = read(fd, buf, MESSAGE_LEN);
    printf("Read data[size: %d]: '%s'\n", ret, buf);

    ret = close(fd);
    if(ret < 0){
        perror("Closing for read error\n");
        return errno;
    }

    return 0;
}