#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

uint8_t lmip = 20;

int main(int argc, char *argv[]) {
	int lconn = socket(AF_UNIX, SOCK_SEQPACKET, 0);

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

	char *towr;
	uint16_t port = 3456;

	towr = &port;
	write(lconn, towr, 2);
	return 0;
}