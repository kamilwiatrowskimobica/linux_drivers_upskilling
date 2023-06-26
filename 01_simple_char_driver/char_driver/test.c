#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main()
{
	int result;
	int fdr = open("/dev/mobica", O_RDONLY);
	int fdw = open("/dev/mobica", O_WRONLY);
	char buffer[16];

	result = write(fdw, "abcdefghij", 10);
	printf("write result: %d\n", result);

	result = read(fdr, buffer, 4);
	printf("read result: %d\n", result);

	result = write(fdw, "abcdefghij", 10);
	printf("write result: %d\n", result);

	result = write(fdw, "abcdefghij", 10);
	printf("write result: %d\n", result);

	result = read(fdr, buffer, 16);
	printf("read result: %d\n", result);

	result = write(fdw, "abcdefghij", 10);
	printf("write result: %d\n", result);

	close(fdr);
	close(fdw);

	return 0;
}