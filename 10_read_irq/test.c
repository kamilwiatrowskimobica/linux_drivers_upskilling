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
    
    fd = open("/dev/pm_char_device", O_RDONLY);
        
    if(fd < 0){
        perror("Opening for read error\n");
        return errno;
    } 

    ret = read(fd, buf, MESSAGE_LEN);
    printf("size of read data %d\n", ret);

    if(close(fd)){
        perror("Closing for read error\n");
        return errno;
    }

    return 0;
}