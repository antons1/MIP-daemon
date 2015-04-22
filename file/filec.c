#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../miptpf/miptpproto.h"

uint8_t lmip;

int checkargs(int, char *[]);

int main(int argc, char *argv[]) {
	int lconn = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	lmip = checkargs(argc, argv);
	if(lmip == -1) return 1;

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
	uint16_t dstp = 4365;
	uint16_t cl = 0;
	char *msg = malloc(0);

	miptpCreatepacket(dstm, dstp, cl, msg, &identify);
	write(lconn, (char *)identify, sizeof(struct miptp_packet)+cl);
	
	struct miptp_packet *test;
	if(lmip == 20) dstm = 10;
	else dstm = 20;

	miptpCreatepacket(dstm, 3456, strlen("Hei, dette er en melding"), "Hei, dette er en melding", &test);
	write(lconn, (char *)test, sizeof(struct miptp_packet)+strlen("Hei, dette er en melding"));

	return 0;
}

int checkargs(int argc, char *argv[]) {
	char *errmsg = malloc(1024);
	char *usemsg = malloc(1024);
	int error = 0;

	sprintf(usemsg, "%s [Local MIP] [Filename] [Destination MIP] [Port number]", argv[0]);

	if(argc < 2) {
		sprintf(errmsg, "Too few arguments");
		error = 1;
	}

	int ret = atoi(argv[1]);

	if(error) {
		printf("%s: error: %s\n", argv[0], errmsg);
		printf("%s: usage: %s\n", argv[0], usemsg);
	} 

	free(errmsg);
	free(usemsg);

	if(error) return -1;
	else return ret;
}