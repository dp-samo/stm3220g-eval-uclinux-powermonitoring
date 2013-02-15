/*
 * This is a user-space application that reads /dev/sample
 * and prints the read characters to stdout
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(int argc, char **argv)
{
	char * app_name = argv[0];
	char * dev_name = "/dev/sample";
	int ret = -1;
	int fd = -1;
	int c, x;

	/*
 	 * Open the sample device RD | WR
 	 */
	if ((fd = open(dev_name, O_RDWR)) < 0) {
		fprintf(stderr, "%s: unable to open %s: %s\n", 
			app_name, dev_name, strerror(errno));
		goto Done;
	}

	/*
 	 * Read the sample device byte-by-byte
 	 */
	while (1) {
		if ((x = read(fd, &c, 1)) < 0) {
			fprintf(stderr, "%s: unable to read %s: %s\n", 
				app_name, dev_name, strerror(errno));
			goto Done;
		}	
		if (! x) break;

		/*
		 * Print the read character to stdout
		 */
		fprintf(stdout, "%c", c);
	}

	/*
 	 * If we are here, we have been successful
 	 */
	ret = 0;

Done:
	if (fd >= 0) {
		close(fd);
	}
	return ret;
}
