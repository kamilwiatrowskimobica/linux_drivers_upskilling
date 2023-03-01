#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#include "inc/mobchar_ioctl.h"

void ioctl_test(void);

int main(void)
{
	
	printf("\n\nTesting IOCTL:\n");
	ioctl_test();
	
	printf("\n\nExiting from main program\n");
	return 0;
}

void ioctl_test(void)
{
	int fl;
	int ret = 13;
	int ioctl_err;

	const char *testing_device_zero = "/dev/mobchar0";

	printf("Opening device\n");
	fl = open(testing_device_zero, O_RDWR);

	if (fl < 0)
	{
		printf("Can't open file\n");
		return;
	}

	printf("Hello test\n");
	ioctl_err = ioctl(fl, MOBCHAR_IOC_HELLO, NULL);
	if (ioctl_err) goto ioctl_error;

	ioctl_err = ioctl(fl, MOBCHAR_IOC_WRITE, &ret);
	if (ioctl_err) goto ioctl_error;
	printf("Write = %d\n", ret);

	// change walue
	ret = 2;
	printf("Befor read ioctl_var = %d\n", ret);
	ioctl_err = ioctl(fl, MOBCHAR_IOC_READ, &ret);
	if (ioctl_err) goto ioctl_error;
	printf("After read ioctl_var = %d\n", ret);
	
close:
	close(fl);
	return;
ioctl_error:
	printf("Mobchar ioctl faled, err=%d\n", ioctl_err);
	goto close;
}