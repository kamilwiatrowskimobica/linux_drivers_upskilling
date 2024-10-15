#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MESSAGE_LEN 256

int main(){
    int ret;
    int fd;
    char message[MESSAGE_LEN];
    char tmp[MESSAGE_LEN];
    const char* gen_path = "/dev/pm_char_device_";
    const int path_size = strlen(gen_path) + 2;

    const char* proc_path = "/proc/chardev_iter";

    for(int i = 0; i < 3; i++){
        char path[path_size];
        snprintf(path, path_size, "%s%d", gen_path, i);
        fd = open(path, O_RDWR);
        if(fd < 0){
            perror("Opening error\n");
            return errno;
        }

        printf("enter short test\n");
        scanf("%[^\n]%*c", message);
        
        printf("Write message\n");
        ret = write(fd, message, strlen(message));
        if(ret < 0){
            perror("Writing error\n");
            return errno;
        }

        printf("Set position\n");
        ret = lseek(fd, 0, SEEK_SET);
        if(ret < 0){
            perror("Setting position error\n");
            return errno;
        }

        printf("Read message\n");
        ret = read(fd, tmp, MESSAGE_LEN);
        if(ret < 0){
            perror("Reading error\n");
            return errno;
        }

        printf("Read data: '%s'\n", tmp);
        ret = close(fd);
        if(ret < 0){
            perror("Closing error\n");
            return errno;
        }
    }

    printf("Proc inter test\n");

    fd = open(proc_path, O_RDWR);
    if(fd < 0){
        perror("Opening error\n");
        return errno;
    }

    ret = read(fd, tmp, MESSAGE_LEN);
    if(ret < 0){
        perror("Reading error\n");
        return errno;
    }
    printf("Read data: %s\n", tmp);
    ret = close(fd);
    if(ret < 0){
        perror("Closing error\n");
        return errno;
    }

    printf("End of test\n");
    return 0;
}