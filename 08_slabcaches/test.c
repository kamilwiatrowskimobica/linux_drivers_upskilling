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
#define COMMAND 'k'
#define SLEEP_WRITE_SEC 2
#define POLL_TIMEOUT_MS 1000

struct command_t{
    uint32_t size;
    uint8_t* command;
};

#define COMMAND_WR_PTR _IOW(COMMAND, 1, struct command_t)
#define COMMAND_RD_PTR _IOR(COMMAND, 2, struct command_t) 
#define COMMAND_WR_VAL _IOW(COMMAND, 3, int)
#define COMMAND_RD_VAL _IOR(COMMAND, 4, int) 
#define COMMAND_RESET _IO(COMMAND, 5) 
#define IOCTL_COMMAND_CACHE_WRITE _IOW(COMMAND, 6, char*)
#define IOCTL_COMMAND_CACHE_READ _IOR(COMMAND, 7, char*)

int main(){

    int ret;
    int fd;
    char buf[MESSAGE_LEN];
    char buf2[15];
    
    fd = open("/dev/pm_char_device", O_RDONLY);
        
    if(fd < 0){
        perror("Opening for read error\n");
        return errno;
    } 

    ioctl(fd, COMMAND_RESET);


    for (int i = 0; i < 10; i++){
        sprintf(buf, "cache test %i", i); // size 14 from  i -> 0 - 9
        if(ioctl(fd, IOCTL_COMMAND_CACHE_WRITE, buf) < 0){
            printf("ERROR: IOCTL_COMMAND_CACHE_WRITE\n");
        }

        if(ioctl(fd, IOCTL_COMMAND_CACHE_READ, buf2) < 0){
            printf("ERROR: IOCTL_COMMAND_CACHE_READ\n");
        }

        printf("Write [%s] ?= Read [%s] by pointer\n", buf, buf2);
    }
    ret = close(fd);
    if(ret < 0){
        perror("Closing for read error\n");
        return errno;
    }

    return 0;
}