#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../miptpf/miptpproto.h"
#define MAX_PART_SIZE 1492

uint8_t lmip;
uint8_t dstmip;
uint16_t dstport;
char *filename;
int lconn;

int checkargs(int, char *[]);
int checkNumber(int, char *, int, int);
int senddata(char *, int);

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

	FILE *lf = fopen(filepath, "r");
	if(lf == NULL) {
		perror("FILEC: Error opening file");
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

	char lread;
	char buf[MAX_PART_SIZE];
	int i = 0;
	int error = 0;

	while(1) {
		lread = fgetc(lf);
		if(lread == EOF) {
			printf("FILEC: End of file reached.\n");
			if(i != 0) error = senddata(buf, i);
			break;
		}

		buf[i++] = lread;
		if(i == MAX_PART_SIZE) {
			error = senddata(buf, i);
			i = 0;
			if(error == 0) break;
		}
	}

	sleep(1);
	
	close(lconn);
	fclose(lf);
	free(path);
	free(filepath);

	return 0;
}

int checkNumber(int res, char *arg, int llim, int ulim) {
	if(res < llim || res > ulim) return 1;
	else if(res == 0 && strcmp(arg, "0") != 0) return 1;
	else return 0;
}

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

int senddata(char *data, int length) {
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
		printf("FILEC: Sent %zd bytes of data (said %d, got %d)\n", sb, mp->content_len, length);
		sleep(1);
		return 1;
	}
}