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
#define DEVICE "/dev/hellodrvchar"
static char receive[BUFFER_LENGTH]; ///< The receive buffer from the LKM

int main()
{
	int i, ret, fd, areStringsDifferent;
	// char stringToSend[BUFFER_LENGTH];
	char *stringToSend[] = { "foo\0", "bar\n\0", "slightlylongerstring", "",
				 "\n" };

	printf("Starting device test code example...\n");

	for (i = sizeof(stringToSend) / sizeof(stringToSend[0]) - 1; i >= 0;
	     i--) {
		fd = open(DEVICE,
			  O_RDWR); // Open the device with read/write access
		if (fd < 0) {
			perror("Failed to open the device...");
			return errno;
		}

		//   printf("Type in a short string to send to the kernel module:\n");
		//   scanf("%[^\n]%*c", stringToSend);                // Read in a string (with spaces)
		//           printf("Writing message to the device [%s].\n", stringToSend[i]);
		ret = write(
			fd, stringToSend[i],
			strlen(stringToSend[i])); // Send the string to the LKM
		if (ret < 0) {
			perror("Failed to write the message to the device.");
			return errno;
		}

		//printf("Press ENTER to read back from the device...\n");
		//getchar();
		sleep(1);

		//           printf("Reading from the device...\n");
		ret = read(fd, receive,
			   BUFFER_LENGTH); // Read the response from the LKM
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		//           printf("The received message is: [%s]\n", receive);

		printf("Wrote [%s].\n", stringToSend[i]);
		printf("Read: [%s]\n", receive);
		areStringsDifferent = strncmp(
			stringToSend[i], receive,
			strnlen(stringToSend[i],
				BUFFER_LENGTH)); //zero if equal compare up to '\0'
		if (areStringsDifferent) {
			printf("FAIL\n");
		} else {
			printf("SUCCESS\n");
		}
		close(fd);
	}
	printf("End of the program\n");
	return 0;
}
