/**
 * @file   mysimplechardev_test.c
 * @author Andrzej Dziarnik
 * @date   23 06 2023
 * @version 0.1
 * @brief  A Linux user space program that communicates with the mysimplechardev.
*/
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_LENGTH 256
#define DEVICES_COUNT 3
#define DEV_NAME_PATTERN "/dev/mysimplechardev_%d"
#define PROC_SIMPLE_FILE "/proc/mysimplechardev_simple"
#define PROC_SEQ_FILE "/proc/mysimplechardev_seq"
#define PROC_LINE_PATTERN "Mysimplechardev_%d, size = %d\n" Kurcz

static int openDev(int *fd, int i)
{
	int ret = 0;
	char devName[64];
	ret = sprintf(devName, DEV_NAME_PATTERN, i);
	if (ret < 0) {
		printf("Prepare dev names failed!");
		return ret;
	}
	ret = 0;
	*fd = open(devName, O_RDWR);
	if (*fd < 0) {
		perror("Failed to open the device...");
		return -1;
	}
	return ret;
}

static int openDevs(int *fds)
{
	int ret = 0;
	for (int i = 0; i < DEVICES_COUNT; i++) {
		ret = openDev(&fds[i], i);
		if (ret < 0)
			break;
	}
	return ret;
}

static int closeDev(int fd)
{
	return close(fd);
}

static int reopenDev(int *fd, int i)
{
	int ret = 0;
	ret = closeDev(*fd);
	if (ret != 0)
		return -1;
	ret = openDev(fd, i);
	return ret;
}

static int closeDevs(int *fds)
{
	int ret = 0;
	for (int i = 0; i < DEVICES_COUNT; i++) {
		ret = closeDev(fds[i]);
		if (ret != 0) {
			perror("Failed to close the device...");
			return errno;
		}
	}
	return ret;
}

static int writeReadTest(int *fds, char stringsToSend[][BUFFER_LENGTH])
{
	int ret = 0;
	char receive[BUFFER_LENGTH];
	for (int i = 0; i < DEVICES_COUNT; i++) {
		memset(receive, 0, BUFFER_LENGTH);
		ret = write(fds[i], stringsToSend[i], strlen(stringsToSend[i]));
		if (ret < 0) {
			perror("Failed to write the message to the device.");
			return errno;
		}
		ret = reopenDev(&fds[i], i);
		if (ret != 0) {
			perror("Failed to reopen the device...");
			return -1;
		}
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		ret = strcmp(stringsToSend[i], receive);
		if (ret != 0)
			return ret;
	}
	return ret;
}

static int writeByteByByteReadTest(int *fds,
				   char stringsToSend[][BUFFER_LENGTH])
{
	int ret = 0;
	char receive[BUFFER_LENGTH];
	for (int i = 0; i < DEVICES_COUNT; i++) {
		memset(receive, 0, BUFFER_LENGTH);
		for (size_t j = 0; j < strlen(stringsToSend[i]); j++) {
			ret = write(fds[i], &stringsToSend[i][j], 1);
			if (ret < 0) {
				perror("Failed to write the message to the device.");
				return errno;
			}
		}
		ret = reopenDev(&fds[i], i);
		if (ret != 0) {
			perror("Failed to reopen the device...");
			return -1;
		}
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		ret = strcmp(stringsToSend[i], receive);
		if (ret != 0)
			return ret;
	}
	return ret;
}

static int writeSeekReadTest(int *fds, char stringsToSend[][BUFFER_LENGTH])
{
	int ret = 0;
	char receive[BUFFER_LENGTH];
	char tmp[BUFFER_LENGTH];
	for (int i = 0; i < DEVICES_COUNT; i++) {
		ret = write(fds[i], stringsToSend[i], strlen(stringsToSend[i]));
		if (ret < 0) {
			perror("Failed to write the message to the device.");
			return errno;
		}
		ret = lseek(fds[i], 10, SEEK_SET);
		if (ret < 0) {
			printf("Failed to seek the device.\n");
			return errno;
		}
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		ret = strcmp(stringsToSend[i] + 10, receive);
		if (ret != 0)
			return ret;
		ret = lseek(fds[i], -10, SEEK_CUR);
		if (ret < 0) {
			printf("Failed to seek the device.\n");
			return errno;
		}
		memset(receive, 0, sizeof(receive));
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		memset(tmp, 0, sizeof(tmp));
		memcpy(tmp, stringsToSend[i], 10);
		ret = strcmp(tmp, receive);
		if (ret != 0)
			return ret;
		ret = write(fds[i], stringsToSend[i], strlen(stringsToSend[i]));
		if (ret < 0) {
			perror("Failed to write the message to the device.");
			return errno;
		}
		ret = lseek(fds[i], -12, SEEK_END);
		if (ret < 0) {
			printf("Failed to seek the device.\n");
			return errno;
		}
		memset(receive, 0, sizeof(receive));
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		ret = strcmp(stringsToSend[i] + (strlen(stringsToSend[i]) - 12),
			     receive);
		if (ret != 0)
			return ret;
		ret = lseek(fds[i], 0, SEEK_SET);
		if (ret < 0) {
			printf("Failed to seek the device.\n");
			return errno;
		}
		memset(receive, 0, sizeof(receive));
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		memset(tmp, 0, sizeof(tmp));
		memcpy(tmp, stringsToSend[i], strlen(stringsToSend[i]) - 12);
		ret = strcmp(tmp, receive);
		if (ret != 0)
			return ret;
	}
	return ret;
}

static int writeSeekWriteSeekReadTest(int *fds,
				      char stringsToSend[][BUFFER_LENGTH])
{
	int ret = 0;
	char receive[BUFFER_LENGTH];
	char tmp[BUFFER_LENGTH];
	for (int i = 0; i < DEVICES_COUNT; i++) {
		memset(receive, 0, BUFFER_LENGTH);
		ret = write(fds[i], stringsToSend[i], strlen(stringsToSend[i]));
		if (ret < 0) {
			perror("Failed to write the message to the device.");
			return errno;
		}
		ret = lseek(fds[i], 0, SEEK_END);
		if (ret < 0) {
			printf("Failed to seek the device.\n");
			return errno;
		}
		ret = write(fds[i], stringsToSend[i], strlen(stringsToSend[i]));
		if (ret < 0) {
			perror("Failed to write the message to the device.");
			return errno;
		}
		ret = lseek(fds[i], 0, SEEK_SET);
		if (ret < 0) {
			printf("Failed to seek the device.\n");
			return errno;
		}
		memset(receive, 0, BUFFER_LENGTH);
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%s%s", stringsToSend[i], stringsToSend[i]);
		ret = strcmp(tmp, receive);
		if (ret != 0)
			return ret;
	}
	return ret;
}

static int procSimpleTest(int *fds, char stringsToSend[][BUFFER_LENGTH])
{
	int ret = 0;
	char receive[BUFFER_LENGTH];
	char expectedProcValue[BUFFER_LENGTH];
	int procFd = open(PROC_SIMPLE_FILE, O_RDONLY);
	if (procFd < 0) {
		perror("Failed to open the proc file...");
		return -1;
	}
	memset(expectedProcValue, 0, BUFFER_LENGTH);
	for (int i = 0; i < DEVICES_COUNT; i++) {
		char tmp[30] = "";
		snprintf(tmp, sizeof(tmp), "Mysimplechardev_%d, size = 0\n", i);
		strncat(expectedProcValue, tmp, sizeof(expectedProcValue) - 1);
	}
	strncat(expectedProcValue,
		"Finished mysimplechardev_read_procsimple!\n",
		sizeof(expectedProcValue) - 1);
	ret = read(procFd, receive, BUFFER_LENGTH);
	if (ret < 0) {
		perror("Failed to read message from the proc file.");
		return errno;
	}
	ret = strcmp(expectedProcValue, receive);
	if (ret != 0) {
		perror("Content of the proc file is incorrect");
		return ret;
	}
	for (int i = 0; i < DEVICES_COUNT; i++) {
		ret = write(fds[i], stringsToSend[i], strlen(stringsToSend[i]));
		if (ret < 0) {
			perror("Failed to write the message to the device.");
			return errno;
		}
		ret = reopenDev(&fds[i], i);
		if (ret != 0) {
			perror("Failed to reopen the device...");
			return -1;
		}
	}
	memset(expectedProcValue, 0, BUFFER_LENGTH);
	for (int i = 0; i < DEVICES_COUNT; i++) {
		char tmp[30] = "";
		snprintf(tmp, sizeof(tmp), "Mysimplechardev_%d, size = %ld\n",
			 i, strlen(stringsToSend[i]));
		strncat(expectedProcValue, tmp, sizeof(expectedProcValue) - 1);
	}
	strncat(expectedProcValue,
		"Finished mysimplechardev_read_procsimple!\n",
		sizeof(expectedProcValue) - 1);
	memset(receive, 0, BUFFER_LENGTH);
	close(procFd);
	procFd = open(PROC_SIMPLE_FILE, O_RDONLY);
	if (procFd < 0) {
		perror("Failed to open the proc file...");
		return -1;
	}
	ret = read(procFd, receive, BUFFER_LENGTH);
	if (ret < 0) {
		perror("Failed to read message from the proc file.");
		return errno;
	}
	ret = strcmp(expectedProcValue, receive);
	if (ret != 0) {
		perror("Content of the proc file is incorrect");
		return ret;
	}
	for (int i = 0; i < DEVICES_COUNT; i++) {
		memset(receive, 0, BUFFER_LENGTH);
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		ret = strcmp(stringsToSend[i], receive);
		if (ret != 0)
			return ret;
	}
	close(procFd);
	return ret;
}

static int procSequenceTest(int *fds, char stringsToSend[][BUFFER_LENGTH])
{
	int ret = 0;
	char receive[BUFFER_LENGTH];
	char expectedProcValue[BUFFER_LENGTH];
	int procFd = open(PROC_SEQ_FILE, O_RDONLY);
	if (procFd < 0) {
		perror("Failed to open the proc file...");
		return -1;
	}
	memset(expectedProcValue, 0, BUFFER_LENGTH);
	for (int i = 0; i < DEVICES_COUNT; i++) {
		char tmp[30] = "";
		snprintf(tmp, sizeof(tmp), "Mysimplechardev_%d, size = 0\n", i);
		strncat(expectedProcValue, tmp, sizeof(expectedProcValue) - 1);
	}
	strncat(expectedProcValue, "Stopping proc seq access!\n",
		sizeof(expectedProcValue) - 1);
	ret = read(procFd, receive, BUFFER_LENGTH);
	if (ret < 0) {
		perror("Failed to read message from the proc file.");
		return errno;
	}
	ret = strcmp(expectedProcValue, receive);
	if (ret != 0) {
		perror("Content of the proc file is incorrect");
		return ret;
	}
	for (int i = 0; i < DEVICES_COUNT; i++) {
		ret = write(fds[i], stringsToSend[i], strlen(stringsToSend[i]));
		if (ret < 0) {
			perror("Failed to write the message to the device.");
			return errno;
		}
		ret = reopenDev(&fds[i], i);
		if (ret != 0) {
			perror("Failed to reopen the device...");
			return -1;
		}
	}
	memset(expectedProcValue, 0, BUFFER_LENGTH);
	for (int i = 0; i < DEVICES_COUNT; i++) {
		char tmp[30] = "";
		snprintf(tmp, sizeof(tmp), "Mysimplechardev_%d, size = %ld\n",
			 i, strlen(stringsToSend[i]));
		strncat(expectedProcValue, tmp, sizeof(expectedProcValue) - 1);
	}
	strncat(expectedProcValue, "Stopping proc seq access!\n",
		sizeof(expectedProcValue) - 1);
	memset(receive, 0, BUFFER_LENGTH);
	close(procFd);
	procFd = open(PROC_SEQ_FILE, O_RDONLY);
	if (procFd < 0) {
		perror("Failed to open the proc file...");
		return -1;
	}
	ret = read(procFd, receive, BUFFER_LENGTH);
	if (ret < 0) {
		perror("Failed to read message from the proc file.");
		return errno;
	}
	ret = strcmp(expectedProcValue, receive);
	if (ret != 0) {
		perror("Content of the proc file is incorrect");
		return ret;
	}
	for (int i = 0; i < DEVICES_COUNT; i++) {
		memset(receive, 0, BUFFER_LENGTH);
		ret = read(fds[i], receive, BUFFER_LENGTH);
		if (ret < 0) {
			perror("Failed to read the message from the device.");
			return errno;
		}
		ret = strcmp(stringsToSend[i], receive);
		if (ret != 0)
			return ret;
	}
	close(procFd);
	return ret;
}

static void printStatus(char *msg, int retCode)
{
	if (retCode)
		printf("%s FAILED!\n", msg);
	else
		printf("%s SUCCESS\n", msg);
}

int main()
{
	int ret = 0;
	char stringsToSend[DEVICES_COUNT][BUFFER_LENGTH] = {
		"This is the string for first device",
		"This is the string for second device",
		"This is the string for third device"
	};
	int fds[DEVICES_COUNT];
	printf("Starting device test code example...\n");
	ret = openDevs(fds);
	printStatus("Open devices", ret);
	if (ret != 0) {
		return ret;
	}
	ret = writeReadTest(fds, stringsToSend);
	printStatus("Write and read test", ret);
	ret = writeByteByByteReadTest(fds, stringsToSend);
	printStatus("Write byte by byte and read test", ret);
	ret = writeSeekReadTest(fds, stringsToSend);
	printStatus("Write, seek and read test", ret);
	ret = writeSeekWriteSeekReadTest(fds, stringsToSend);
	printStatus("Write, seek write and read test", ret);
	ret = procSimpleTest(fds, stringsToSend);
	printStatus("Proc simple test", ret);
	ret = procSequenceTest(fds, stringsToSend);
	printStatus("Proc sequence test", ret);

	ret = closeDevs(fds);
	printStatus("Close devices", ret);
	return ret;
}