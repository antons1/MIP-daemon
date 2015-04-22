#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#define MAX_PORTS 10
#define WINDOW_SIZE 10
#define FDPOS_MIP 0
#define FDPOS_APP MAX_PORTS+1
#define MAX_QUEUE 10
#define TPID 2
#define TP_MAX_DATA 1492

#include "miptpproto.h"
#include "../mipdf/mipdproto.h"
#include "tpproto.h"
#include "packetlist.h"
#include "applist.h"
#include "gbn.h"

uint8_t checkargs(int, char *[]);
int mipConnect();
int appConnect();

uint8_t debug;
uint8_t lmip;
uint16_t timeout;

int getNextFds(struct pollfd []);

int main(int argc, char *argv[]) {
	lmip = checkargs(argc, argv);
	if(lmip == 0) return 1;

	// Connecting to MIP and setting upp App listening socket
	int mipfd = mipConnect();
	int appfd = appConnect();

	if(mipfd == -1) {
		if(debug) fprintf(stderr, "MIPTP: Connection to daemon failed. Exiting.\n");
		if(debug) perror("Connecting to MIP");
		return 1;
	}

	if(appfd == -1) {
		fprintf(stderr, "MIPTP: Creating listening socket failed. Exiting.\n");
		if(debug) perror("Setting up app socket");
		return 1;
	}

	// Setting up FDs for poll
	struct pollfd fds[MAX_PORTS+2];
	fds[FDPOS_MIP].fd = mipfd;
	fds[FDPOS_MIP].events = POLLIN | POLLOUT | POLLHUP;

	fds[FDPOS_APP].fd = appfd;
	fds[FDPOS_APP].events = POLLIN;

	int i = 1;
	for(; i <= MAX_PORTS; i++) {
		fds[i].fd = -1;
		fds[i].events = 0;
	}

	// Start polling
	while(1) {
		nfds_t polls = MAX_PORTS+2;
		int rd = poll(fds, polls, 0);

		if(rd == -1) {
			perror("MIPTP: Poll error");
			continue;
		}

		if(rd != 0) {
			if(fds[FDPOS_MIP].revents & POLLHUP) {
				// MIP has disconnected
				if(debug) fprintf(stderr, "MIPTP: MIP daemon disconnected\n");
				
				break;
			}

			if(fds[FDPOS_MIP].revents & POLLIN) {
				// Message from MIP
				if(debug) fprintf(stderr, "MIPTP: Data from MIP daemon\n");
				char buf[1500];
				recv(fds[FDPOS_MIP].fd, buf, 1500, 0);
				struct mipd_packet *mp = (struct mipd_packet *)buf;
				recvMip(mp);
			}

			if(fds[FDPOS_APP].revents & POLLIN) {
				// Incoming connection from app
				char buf[MIPTP_MAX_CONTENT_LEN+sizeof(struct miptp_packet)];
				int fd = accept(fds[FDPOS_APP].fd, NULL, NULL);
				ssize_t rb = read(fd, buf, MIPTP_MAX_CONTENT_LEN);
				struct miptp_packet *mtp = malloc(rb);

				memset(mtp, 0, rb);
				memcpy(mtp, buf, rb);

				if(debug) fprintf(stderr, "MIPTP: Incoming connection from app, port %d.", mtp->dst_port);

				if(getApp(mtp->dst_port, NULL) == 0) {
					if(debug) fprintf(stderr, " Accepted\n");
					int fdind = getNextFds(fds);

					if(fdind == 0) {
						if(debug) fprintf(stderr, "MIPTP: No room in FD set, closing\n");
						close(fd);
					} else {
						fds[fdind].fd = fd;
						fds[fdind].events = POLLIN | POLLOUT | POLLHUP;
					}

					addApp(mtp->dst_port, fdind, NULL);
				} else {
					if(debug) fprintf(stderr, " Rejected, port is in use\n");
					close(fd);
				}
			}

			struct applist *curr = NULL;
			while(getNextApp(&curr) != 0) {
				if(fds[curr->fdind].revents & POLLHUP) {
					// App has disconnected
					if(debug) fprintf(stderr, "MIPTP: App on port %d has disconnected\n", curr->port);

					close(fds[curr->fdind].fd);
					fds[curr->fdind].fd = -1;
					rmApp(curr->port);
					fds[curr->fdind].revents = 0;
				}

				if(fds[curr->fdind].revents & POLLIN) {
					// Incoming data on port
					if(debug) fprintf(stderr, "MIPTP: Incoming data on port %d\n", curr->port);
					char buf[TP_MAX_DATA+sizeof(struct miptp_packet)];
					recv(fds[curr->fdind].fd, buf, TP_MAX_DATA+sizeof(struct miptp_packet), 0);
					
					struct miptp_packet *recvd = (struct miptp_packet *)buf;
					recvApp(recvd, curr);
				}

				if((fds[curr->fdind].revents & POLLOUT) && hasRecvData(curr)) {
					// Port ready for write, and has waiting data
					struct miptp_packet *mp;
					getAppPacket(&mp, curr);
					send(fds[curr->fdind].fd, mp, mp->content_len, 0);
				}

				if((fds[FDPOS_MIP].revents & POLLOUT) && hasSendData(curr)) {
					// Mip ready for write, and port has data
					struct mipd_packet *mp;
					getMipPacket(&mp, curr);
					send(fds[FDPOS_MIP].fd, mp, mp->content_len, 0);
				}

				curr = NULL;
			}
		}
	}

	return 0;
}

/**
 * Checks arguments given on the command line
 * @param  argc Argument count
 * @param  argv Arguments
 * @return      Returns the local MIP address, or 0 on error
 */
uint8_t checkargs(int argc, char *argv[]) {
	uint8_t error = 0;
	uint8_t lmip = 0;
	if(argc < 3) error = 1;
	else if(argc >= 3) {
		lmip = atoi(argv[1]);
		if(lmip == 0) error = 2;

		timeout = atoi(argv[2]);
		if(timeout == 0 || atoi(argv[2]) > 65535) error = 3;
	}

	if(strcmp(argv[argc-1], "-d") == 0) debug = 1;

	if(error == 0) return lmip;
	else {
		char *errmsg = malloc(512);
		char *usage = malloc(512);
		sprintf(usage, "%s [MIP-address] [Timeout] [-d]", argv[0]);

		if(error == 1) sprintf(errmsg, "Too few arguments.");
		else if(error == 2) sprintf(errmsg, "MIP address must be between 1 and 254. %s is invalid.", argv[1]);
		else if(error == 3) sprintf(errmsg, "Timeout must be between 1 and 65535 seconds. %s is invalid.", argv[2]);

		fprintf(stderr, "%s: error: %s\n", argv[0], errmsg);
		fprintf(stderr, "%s: usage: %s\n", argv[0], usage);

		free(errmsg);
		free(usage);

		return 0;
	}
}

/**
 * Connects to the local MIP daemon
 * @return File Descriptor for daemon socket
 */
int mipConnect() {
	if(debug) fprintf(stderr, "MIPTP: Connecting to MIP socket\n");
	int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if(fd == -1) {
		perror("MIPTP: Error creating MIP socket");
		return -1;
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	sprintf(addr.sun_path, "socket%d", lmip);

	if(connect(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
		perror("MIPTP: Error connecting to MIP socket");
		return -1;
	}

	struct mipd_packet *identify;
	mipdCreatepacket(TPID, 0, "", &identify);

	identify->dst_mip = TPID;

	int wb = write(fd, (char *)identify, sizeof(struct mipd_packet));
	if(wb == -1) {
		perror("Failed writing to MIP");
		return 0;
	}
	free(identify);

	return fd;
}

/**
 * Sets up socket for listening on local applications
 * @return File Descriptor for app socket
 */
int appConnect() {
	if(debug) fprintf(stderr, "MIPTP: Setting up App socket\n");

	int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if(fd == -1) {
		perror("MIPTP: Error creating app socket");
		return -1;
	}

	struct sockaddr_un addr;
	addr.sun_family = AF_UNIX;
	sprintf(addr.sun_path, "socket%d_tp", lmip);

	unlink(addr.sun_path);

	if(bind(fd, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) == -1) {
		perror("MIPTP: Error binding to app socket");
		return -1;
	}

	if(listen(fd, MAX_QUEUE) == -1) {
		perror("MIPTP: Error listening to app socket");
		return -1;
	}

	return fd;
}

/**
 * Returns the index of the next available spot in the FD set
 * @param  fds FD set to find position in
 * @return     Index of free position, 0 if none available
 */
int getNextFds(struct pollfd fds[]) {
	int i = 1;
	for(; i <= MAX_PORTS; i++) {
		if(fds[i].fd == -1) return i;
	}

	return 0;
}