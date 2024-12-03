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

void* update_file(void* arg){
    int ret;
    int fd;
    char message[MESSAGE_LEN];
    struct pollfd poll_data;
    int i = 0;

    fd = open("/dev/pm_char_device", O_WRONLY | O_APPEND);
    if(fd < 0){
        perror("Opening for write error\n");
        return NULL;
    } 

    poll_data.fd = fd;
    poll_data.events = POLLOUT | POLLWRNORM ;

    while(1){
        ret = poll(&poll_data, 1, POLL_TIMEOUT_MS);
        if(ret < 0){
            printf("poll write error \n");
            break;
        }

        if(ret = 0){
            printf("poll write timeout\n");
            break;
        }

        if((poll_data.revents & POLLERR ) == POLLERR){
            printf("POLLERR\n");
            break;
        }

        if((poll_data.revents & (POLLOUT | POLLWRNORM)) == (POLLOUT | POLLWRNORM)){
            snprintf(message, MESSAGE_LEN, "update value: %d", i);
            ret = write(fd, message, strlen(message));
            if(ret < 0){
                perror("Writting error\n");
                return NULL;
            } 

            i++;
        }
        sleep(SLEEP_WRITE_SEC);
    }

    ret = close(fd);
    if(ret < 0){
        perror("Closing for write error\n");
        return NULL;
    }
}

int main(){

    int ret;
    int fd;
    char buf[MESSAGE_LEN];
    struct pollfd poll_data;

    pthread_t write_task;
    
    fd = open("/dev/pm_char_device", O_RDONLY);
        
    if(fd < 0){
        perror("Opening for read error\n");
        return errno;
    } 

    ioctl(fd, COMMAND_RESET);

    poll_data.fd = fd;
    poll_data.events = POLLIN | POLLRDNORM ;

    pthread_create(&write_task, NULL, update_file, "write_task");

    while(1){
        ret = poll(&poll_data, 1, POLL_TIMEOUT_MS);
        if(ret < 0){
            printf("poll read error \n");
            break;
        }

        if(ret = 0){
            printf("poll read timeout\n");
            break;
        }

        if((poll_data.revents & POLLERR) == POLLERR){
            printf("POLLERR\n");
            break;
        }

        if((poll_data.revents & (POLLIN | POLLRDNORM)) == (POLLIN | POLLRDNORM)){
            memset(buf, '\0', MESSAGE_LEN);
            ret = read(fd, buf, MESSAGE_LEN);
            if(ret < 0){
                perror("Reading error\n");
                return NULL;
            }
            buf[ret] = '\0';
            printf("data: %s \n", buf);
        }
    }

    ret = close(fd);
    if(ret < 0){
        perror("Closing for read error\n");
        return errno;
    }

    pthread_join(write_task, NULL);

    return 0;
}