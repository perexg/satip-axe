/*
 * Downloaded from:
 * https://github.com/akosinov/unicable/blob/master/dsqsend/senddsq.c
 *
 * Thanks to akosinov
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <stdint.h>
#include <linux/dvb/frontend.h>


int dsend(int fd, char *data) {
	struct timespec req = { 0, 1000000 * 100 };
	struct dvb_diseqc_master_cmd cmd;
	cmd.msg_len = 0;
	int p = 0, processed;
	unsigned char d;

	while(sscanf(&data[p], "%02hhx %n", &d, &processed) == 1) {
		if(cmd.msg_len < sizeof(cmd.msg)) {
			printf("%02X ", d);
			cmd.msg[cmd.msg_len++] = d;
			p += processed;
		} else {
			printf("Too much data in line\n");
			return -1;
		}
	}
	printf("\n");

	if (ioctl(fd, FE_DISEQC_SEND_MASTER_CMD, &cmd)) {
		printf("DiseqC sending error\n");
		return -1;
	}

	while (nanosleep(&req, &req));
	return 0;
}

int main (int argc, char **argv) {
	char frontend_devname[80], *filename = NULL;
	int frontend = 0, opt, fd;
	FILE *in = stdin;
	char buf[64];

	while ((opt = getopt(argc, argv, "f:i:h?")) != -1) {
		switch (opt)  {
			case 'i':
				filename = optarg;
				break;
			case 'f':
				frontend = strtoul(optarg, NULL, 0);
				break;
			default:
				printf("USAGE: %s [-f <frontend nr>][-i <filename>]\n", argv[0]);
				return -1;
		}
	}

	if (filename && (!(in = fopen(filename, "rt")))) {
		printf("failed to open '%s'\n", filename);
		return -1;
	}

	snprintf (frontend_devname, sizeof(frontend_devname),
		"/dev/axe/frontend-%i", frontend);
	printf("using '%s' \n", frontend_devname);

	if ((fd = open (frontend_devname, O_RDWR | O_NONBLOCK)) < 0) {
		printf("failed to open '%s': %d\n", frontend_devname, errno);
		return -1;
	}

	if (ioctl(fd, FE_SET_TONE, SEC_TONE_OFF)) {
		printf("Setting tone error\n");
		return -1;
	}
	
	while(fgets(buf, sizeof(buf), in) && (!dsend(fd, buf))) {
	}
	if(ferror(in)) {
		printf("Read error\n");
		return -1;
	}

	if(filename)
		fclose(in);
	close (fd);
	return 0;
}

