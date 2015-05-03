#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../miptpf/miptpproto.h"
#define MAX_PART_SIZE 1492

uint8_t lmip;
uint8_t dstmip;
uint16_t dstport;
char *filename;
int lconn;
int i = 0;

int checkargs(int, char *[]);
int checkNumber(int, char *, int, int);
int senddata(char *, ssize_t);

/**
 * Main function for file client
 * @param  argc Number of arguments
 * @param  argv Arguments given
 * @return      0 on success
 */
int main(int argc, char *argv[]) {
	int args = checkargs(argc, argv);
	if(args == -1) return 1;

	char *path = malloc(256);
	memset(path, 0, 256);
	if(getcwd(path, 256) == NULL) {
		perror("FILEC: Error getting working directory");
		return 1;
	}

	char *filepath = malloc(strlen(path)+strlen(filename)+2);
	sprintf(filepath, "%s/%s", path, filename);
	printf("FILEC: Opening file %s\n", filepath);

	int filefd = open(filepath, O_RDONLY);
	if(filefd == -1) {
		perror("FILEC: Opening file");
		return 1;
	}

	lconn = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if(lconn == -1) {
		perror("FILEC: Error creating to socket");
		return 1;
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	sprintf(addr.sun_path, "socket%d_tp", lmip);

	if(connect(lconn, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) == -1) {
		perror("FILEC: Error connecting to socket");
		return 1;
	}

	struct miptp_packet *identify;
	uint8_t dstm = lmip;
	uint16_t dstp = dstport;
	uint16_t cl = 0;
	char *msg = malloc(0);

	miptpCreatepacket(dstm, dstp, cl, msg, &identify);
	write(lconn, (char *)identify, sizeof(struct miptp_packet)+cl);
	free(identify);

	char buf[MAX_PART_SIZE];
	int error = 0;
	while(1) {
		ssize_t rb = read(filefd, buf, MAX_PART_SIZE);
		if(rb == -1) {
			perror("FILEC: Reading file");
			break;
		} else if(rb == 0) {
			printf("FILEC: End of file reached\n");
			break;
		} else {
			error = senddata(buf, rb);
			if(error == 0) {
				printf("FILEC: Error sending data\n");
				break;
			}
		}
	}

	close(lconn);
	close(filefd);
	free(path);
	free(filepath);

	return 0;
}

/**
 * Checks a number agains a string to check if the parsed number is valid, and perform error checking lacking in atoi
 * @param  res  Number to check
 * @param  arg  String to check against
 * @param  llim Lower limit of accepted value
 * @param  ulim Upper limit of accepted value
 * @return      Returns 0 if OK, 1 if not
 */
int checkNumber(int res, char *arg, int llim, int ulim) {
	if(res < llim || res > ulim) return 1;
	else if(res == 0 && strcmp(arg, "0") != 0) return 1;
	else return 0;
}

/**
 * Checks arguments given to program
 * @param  argc Number of arguments
 * @param  argv Arguments given
 * @return      1 if OK, 0 if not
 */
int checkargs(int argc, char *argv[]) {
	char *errmsg = malloc(1024);
	char *usemsg = malloc(1024);
	int error = 0;

	sprintf(usemsg, "%s [Local MIP] [Filename] [Destination MIP] [Port number]", argv[0]);

	if(argc < 5) {
		sprintf(errmsg, "Too few arguments");
		error = 1;
	} else {
		lmip = atoi(argv[1]);
		filename = argv[2];
		dstmip = atoi(argv[3]);
		dstport = atoi(argv[4]);

		error = checkNumber(lmip, argv[1], 1, 254);
		if(error != 0) sprintf(errmsg, "Local MIP must be an integer between 1 and 254, %s is invalid\n", argv[1]);
		else if((error = checkNumber(dstmip, argv[3], 1, 254)) != 0) sprintf(errmsg, "Destination MIP must be an integer between 1 and 254, %s is invalid", argv[3]);
		else if((error = checkNumber(dstport, argv[4], 1, 65535)) != 0) sprintf(errmsg, "Port number must be an integer between 1 and 65535. %s is invalid", argv[4]);

	}

	if(error) {
		printf("%s: error: %s\n", argv[0], errmsg);
		printf("%s: usage: %s\n", argv[0], usemsg);
	} 

	free(errmsg);
	free(usemsg);

	if(error) return -1;
	else return 1;
}

/**
 * Sends a message to the local MIP daemon
 * @param  data   Data to send
 * @param  length Length of data
 * @return        1 on success, 0 on error
 */
int senddata(char *data, ssize_t length) {
	struct timespec waiter;

	waiter.tv_sec = 0;
	waiter.tv_nsec = 200000000;

	struct miptp_packet *mp;
	int create = miptpCreatepacket(dstmip, dstport, length, data, &mp);

	ssize_t sb = 0;
	if(create) sb = send(lconn, mp, length+sizeof(struct miptp_packet), 0);

	if(sb <= 0) {
		perror("FILEC: Error sending data");
		return 0;
	} else if(!create) {
		printf("FILEC: Error creating MIPtp packet\n");
		return 0;
	} else {
		printf("FILEC: Seq %d - Sent %zd bytes of data (said %d, got %zd)\n", i++, sb, mp->content_len, length);
		nanosleep(&waiter, NULL);
		return 1;
	}
}