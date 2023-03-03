/**
 * @file   testebbchar.c
 * @author Derek Molloy
 * @date   7 April 2015
 * @version 0.1
 * @brief  A Linux user space program that communicates with the ebbchar.c LKM. It passes a
 * string to the LKM and reads the response from the LKM. For this example to work the device
 * must be called /dev/ebbchar.
 * @see http://www.derekmolloy.ie/ for a full description and follow-up descriptions.
*/
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 256 ///< The buffer length (crude but fine)
			  ///number of read chars from LKM depens on this
#define SEQFILE "/proc/mtle_sequence"

static char receive[BUFFER_LENGTH]; ///< The receive buffer from the LKM

int main()
{
	int i, ret, fd, areStringsDifferent;

	printf("Starting device test code example...\n");

		//printf("Trying to open file %s \n", DEVICE);
		fd = open(SEQFILE,
			  O_RDWR); // Open the device with read/write access
		if (fd < 0) {
			perror("Failed to open the device...");
			return errno;
		}


		//           printf("Reading from the device...\n");
		ret = read(fd, receive,
			   BUFFER_LENGTH); // Read the response from the LKM
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		sleep(2);

		printf("Read: [%s]\n", receive);

		close(fd);

	printf("End of the program\n");
	return 0;
}
