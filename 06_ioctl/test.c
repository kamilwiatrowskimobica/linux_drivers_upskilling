#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>

#define MESSAGE_LEN 256
#define COMMAND 'k'

struct command_t{
    uint32_t size;
    uint8_t* command;
};

#define COMMAND_WR_PTR _IOW(COMMAND, 1, struct command_t)
#define COMMAND_RD_PTR _IOR(COMMAND, 2, struct command_t) 
#define COMMAND_WR_VAL _IOW(COMMAND, 3, int)
#define COMMAND_RD_VAL _IOR(COMMAND, 4, int) 

int main(){

    int ret;
    int fd;
    char *buf = "SEE ME?";
    struct command_t cmd = {
        .size = strlen(buf),
        .command = buf
    };
    int opt = 2;

    int test_opt;
    char test_buf[20] = "\0";

    fd = open("/dev/pm_char_device", O_WRONLY | O_APPEND);
        
    if(fd < 0){
        perror("Opening error\n");
        return errno;
    } 

    // WRITE
    if(ioctl(fd, COMMAND_WR_VAL, opt) < 0){
        printf("ERROR: COMMAND_WR_VAL\n");
    } 
    if(ioctl(fd, COMMAND_WR_PTR, (struct command_t*)&cmd) < 0){
        printf("ERROR: COMMAND_WR_PTR\n");
    }

    // ret = lseek(fd, 0, SEEK_SET);
    // if(ret < 0){
    //     perror("Setting position error\n");
    //     return errno;
    // }

    // READ
    cmd.size = 20; //strlen(buf);
    cmd.command = test_buf;

    test_opt = ioctl(fd, COMMAND_RD_VAL);
    if(ioctl(fd, COMMAND_RD_PTR, (struct command_t*)&cmd) < 0){
        printf("ERROR: COMMAND_RD_PTR\n");
    }

    printf("Write [%i] ?= Read [%i] by value\n", opt, test_opt);
    printf("Write [%s] ?= Read [%s] by pointer\n", buf, test_buf);

    ret = close(fd);
    if(ret < 0){
        perror("Closing error\n");
        return errno;
    }

    return 0;
}