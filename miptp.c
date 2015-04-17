#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "applist.h"

#define MAX_PORTS 65535
#define FDPOS_MIP 0
#define FDPOS_APP MAX_PORTS+1
#define MAX_QUEUE 10

uint8_t checkargs(int, char *[]);
int mipConnect();
int appConnect();

uint8_t debug;
uint8_t lmip;
uint16_t timeout;

int main(int argc, char *argv[]) {
	lmip = checkargs(argc, argv);
	if(lmip == 0) return 1;

	// Connecting to MIP and setting upp App listening socket
	int mipfd = mipConnect();
	int appfd = appConnect();

	if(mipfd == -1) {
		if(debug) fprintf(stderr, "MIPTP: Connection to daemon failed. Continuing.\n");
		mipfd = 0;
		//return 1;
	}

	if(appfd == -1) {
		fprintf(stderr, "MIPTP: Creating listening socket failed. Exiting.\n");
		return 1;
	}

	// Setting up FDs for poll
	struct pollfd fds[MAX_PORTS+2];
	fds[FDPOS_MIP].fd = mipfd;
	fds[FDPOS_MIP].events = POLLIN | POLLOUT | POLLHUP;

	fds[FDPOS_APP].fd = appfd;
	fds[FDPOS_APP].events = POLLIN;

	// Start polling
	while(1) {
		int rd = poll(fds, MAX_PORTS+2, 0);

		if(rd != 0) {
			if(fds[FDPOS_MIP].revents & POLLHUP) {
				// MIP has disconnected
				if(debug) fprintf(stderr, "MIPTP: MIP daemon disconnected\n");
				
				break;
			}

			if(fds[FDPOS_MIP].revents & POLLIN) {
				// Message from MIP
				
				if(debug) fprintf(stderr, "MIPTP: Data from MIP daemon\n");
			}

			if(fds[FDPOS_MIP].revents & POLLOUT) {
				// MIP ready for write
			}

			if(fds[FDPOS_APP].revents & POLLIN) {
				// Incoming connection from app
				char buf[16];
				read(fds[FDPOS_APP].fd, buf, 2);
				if(debug) fprintf(stderr, "MIPTP: Incoming connection from app, port %s.", buf);

				uint16_t port = (uint16_t)atoi(buf);
				if(getApp(port, NULL) == 0) {
					if(debug) fprintf(stderr, " Accepted\n");
					int fd = accept(fds[FDPOS_APP].fd, NULL, NULL);
					fds[port].fd = fd;
					fds[port].events = POLLIN | POLLOUT | POLLHUP;

					addApp(port, NULL);
				} else {
					if(debug) fprintf(stderr, " Rejected, port is in use\n");
					close(accept(fds[FDPOS_APP].fd, NULL, NULL));
				}
			}

			struct applist *curr = NULL;
			getNextApp(&curr);
			while(curr != NULL) {
				if(fds[curr->port].revents & POLLHUP) {
					// App has disconnected
					if(debug) fprintf(stderr, "MIPTP: App on port %d has disconnected\n", curr->port);

					close(fds[curr->port].fd);
					fds[curr->port].fd = -1;
					rmApp(curr->port);
				}

				if(fds[curr->port].revents & POLLIN) {
					// Incoming data on port
					if(debug) fprintf(stderr, "MIPTP: Incoming data on port %d\n", curr->port);
				}

				if(fds[curr->port].revents & POLLOUT) {
					// Port ready for write
				}

				getNextApp(&curr);
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