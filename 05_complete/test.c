#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>

#define MESSAGE_LEN 256

void* update_file(void* arg){
    int ret;
    int fd;
    char message[MESSAGE_LEN];

    for(int i = 0; i < 10; i++){

        fd = open("/dev/pm_char_device", O_WRONLY | O_APPEND);
        
        if(fd < 0){
            perror("Opening error\n");
            return NULL;
        } 

        snprintf(message, MESSAGE_LEN, "%s:%d", (char*)arg, i);
        printf("Write data: '%s'\n", message);
        ret = write(fd, message, strlen(message));
        if(ret < 0){
            perror("Writing error\n");
            return NULL;
        }  

         ret = close(fd);
        if(ret < 0){
            perror("Closing error\n");
            return NULL;
        }
    }
}

void* read_file(void *){
    int ret;
    int fd;
    char tmp[MESSAGE_LEN];

    for( int i = 0; i < 10; i++){
        fd = open("/dev/pm_char_device", O_RDONLY);
        if(fd < 0){
            perror("Opening error\n");
            return NULL;
        }
        ret = lseek(fd, 0, SEEK_SET);
        if(ret < 0){
            perror("Setting position error\n");
            return NULL;
        }

        ret = read(fd, tmp, MESSAGE_LEN);
        if(ret < 0){
            perror("Reading error\n");
            return NULL;
        }

        printf("Read data: '%s'\n", tmp);

        ret = close(fd);
        if(ret < 0){
            perror("Closing error\n");
            return NULL;
        }
    }

    return NULL;
}

int main(){
    pthread_t task1, task5;

    pthread_create(&task1, NULL, update_file, "task_1");
    pthread_create(&task5, NULL, read_file, NULL);
    
    pthread_join(task1, NULL);
    pthread_join(task5, NULL);

    return 0;
}