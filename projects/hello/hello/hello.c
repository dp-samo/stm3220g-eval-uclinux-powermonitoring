#include <stdio.h>
#include <unistd.h>
#include "hello.h"
#include <sys/mount.h>

int main(int argc, char **argv)
{
	char buff[4096];
	int rc;
	FILE *fp;

	printf("Mounting /proc..\n");

	if (mount("proc", "/proc", "proc", MS_MGC_VAL, NULL)) {
		perror("Unable to mount /proc");
		goto xit;
	}
	if (!(fp = fopen("/proc/meminfo", "r"))) {
		perror("fopen");
		goto xit;
	}
	printf("Reading /proc/meminfo:\n");
	while (fgets(buff, sizeof(buff), fp)) {
		fputs(buff, stdout);
	}
	printf("Done\n");

	while(1) {
		printf(HELLO_STRING);
		sleep(3);
	}
xit:
	return -1;
}
