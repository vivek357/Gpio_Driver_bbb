/* Description : This is a test application which is used for testing
* GPIO input functionality of the bbb-gpio Linux device driver
* implemented for BBB platform. The test application first sets
* all the GPIO pins on the BBB to input, then it reads all the 
* GPIO pins logic level and print these value to the terminal.
*/


#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>


#define NUM_GPIO_PINS 2
#define MAX_GPIO_NUMBER 115
#define BUF_SIZE 3
#define PATH_SIZE 20


int main(int argc, char **argv)
{
	int i = 0, index = 0;
	int fd[NUM_GPIO_PINS];
	char path[PATH_SIZE];
	char buf[BUF_SIZE];
	char readBuf[2];
// Open all GPIO pins
	for (i = 0; i < MAX_GPIO_NUMBER; i++) 
	{
		/*if (i==20 || i==26 || i==27 || i==44 || 
		    i==45 || i==46 || i==47 || i==48 ||
		    i==49 || i==60 || i==65 || i==66 ||
		    i==67 || i==68 || i==69 || i==81 ||
		    i==112 || i==115 || i==117)*/
		if(i==48)
		{
			snprintf(path, sizeof(path), "/dev/bbbGpio%d", i);
			fd[index] = open(path, O_RDWR);
			if (fd[index] < 0)
			{
				perror("Error opening GPIO pin");
				exit(EXIT_FAILURE);
			}
			index++;
		}
	}
// Set direction of GPIO pins to input
	printf("Set pins to input\n");
	strncpy(buf, "in", 2);
	buf[2] = '\0';
	for (index = 0; index < NUM_GPIO_PINS; index++)
	{
		if (write(fd[index], buf, sizeof(buf)) < 0) 
		{
			perror("write, set pin input");
			exit(EXIT_FAILURE);
		}
	}
// Read logic level of GPIO pins and display them to the terminal
	index = 0;
	for (i = 0; i < MAX_GPIO_NUMBER; i++)
	{
		/*if (i==20 || i==26 || i==27 || i==44 || 
		    i==45 || i==46 || i==47 || i==48 ||
		    i==49 || i==60 || i==65 || i==66 ||
		    i==67 || i==68 || i==69 || i==81 ||
		    i==112 || i==115 || i==117)*/
		if(i==48)
		{
			if (read(fd[index], readBuf, 1) < 1) 
			{
				perror("write, set pin input");
				exit(EXIT_FAILURE);
			}
			readBuf[1] = '\0';
			printf("GPIO pin: %d Logic level: %s\n", i, readBuf);
			index++;
		}
	}
	return EXIT_SUCCESS;
}
