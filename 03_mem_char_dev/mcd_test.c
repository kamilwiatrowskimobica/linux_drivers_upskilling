#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

int main() {

    int fd, result;

    printf("\n-- TEST mem char drv--\n");
    /* Open operation */
    if ((fd = open("/dev/mcd_dev0", O_RDWR)) < 0 ) {
        perror("1. open failed \n");
        goto fail;
    }
    else{
            printf("file opend\n");
    }

    /* Write operation */

    char *w_b = "ABCDEFGHIJ";
    int len = strlen(w_b);
    result = write(fd, (void*) w_b, len);
    if ( result != len ){
        printf("Oh dear, something went wrong with write()! %s\n", strerror(errno));
    }
    else{
        printf("write operation executed succesfully\n");
    }
    if (close(fd)){
        perror("1. close failed \n");
    goto fail;
    }
    else{
        printf("file closed\n");
    }
    /* Read operation */

    if ((fd = open("/dev/mcd_dev0", O_RDWR)) < 0 ) {
        perror("1. open failed \n");
        goto fail;
    }
    else{
            printf("file opend\n");
    }

    char a[20];
    char b[20];
    char c[20];
    //reading a
    result = read(fd, (void*)a, 3);
    if ( result != 3 && a[0] == 'A'){
        printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
    }
    else{
        printf("read operation executed succesfully\n");
        printf("string read %s \n",a);
    }
    //reading b
    result = read(fd, (void*)b, 5);
    if ( result != 5 && b[0] == 'D'){
        printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
    }
    else{
        printf("read operation executed succesfully\n");
        printf("string read %s \n",b);
    }
    //reading c
    lseek(fd, 2, SEEK_SET);
    result = read(fd, (void*)c, 20);
    if ( result != 8 && c[0] == 'C'){
        printf("Oh dear, something went wrong with read()! %s\n", strerror(errno));
    }
    else{
        printf("read operation executed succesfully\n");
        printf("string read %s \n",c);
    }


    /* Close operation */
    if (close(fd)){
		    perror("1. close failed \n");
        goto fail;
		}
		else{
		    printf("file closed\n");
		}
		printf("-- TEST PASSED --\n");
    return 0;
    fail:
    printf("-- TEST FAILED --\n");
    return -1;

}