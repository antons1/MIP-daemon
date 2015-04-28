#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netpacket/packet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <sys/un.h>

// Local
#include "mip.h"
#include "unixs.h"
#include "mipmac.h"
#include "comm.h"
#include "../rdf/rd.h"
#include "mipdproto.h"

// Definitions
#define MAX_SOCKETS 5	// Maximum number of non-ethernet sockets that can be in the POLL array
#define STDIN_FD 0		// FD of STDIN
#define STDOUT_FD 1		// FD of STDOUT
#define STDERR_FD 2		// FD of STDERR
#define MAX_MIPS 10		// Maximum amount of MIP addresses suported by the daemon 
#define RDID 1
#define TPID 2

// Prototypes
int setupethsocket(struct pollfd *);
int setupunixsocket();
int checkargs(int, char *[]);
void printhwaddr(char *);

char debug = 0;				// Whether debug mode is active
uint8_t mipaddrs[MAX_MIPS];	// MIP address of this daemon
int nomips = 0;

/**
 * Main
 * @param  argc Argument count
 * @param  argv Arguments
 * @return      0 on success, something else on fail
 */
int main(int argc, char *argv[]) {
	// Check arguments
	nomips = checkargs(argc, argv);
	if(nomips == -1) return 0;

	if(debug) {
		fprintf(stderr, "MIPD: %s started with MIP addresses:", argv[0]);
		int i = 0;
		for(;i < nomips; i++) 
			fprintf(stderr, " %d", mipaddrs[i]);
		fprintf(stderr, "\n");
		fprintf(stderr, "MIPD: Debug mode active\n");
	}

	// Setup unix listening socket
	int unixsocket = setupunixsocket();
	if(unixsocket < 0) return 0;

	// Setup poll
	struct pollfd fds[MAX_SOCKETS+MAX_MIPS];

	memset(fds, 0, sizeof(fds));

	// Ethernet socket (0-MAX_MIPS)
	// Get socket FDs
	int noofsockets = setupethsocket(fds);
	if(noofsockets == -1) {
		fprintf(stderr, "MIPD: No ethernet sockets were created.\n");
		return 0;
	}

	// STDIN
	fds[MAX_MIPS+0].fd = STDIN_FD;
	fds[MAX_MIPS+0].events = POLLIN;

	// Unix domain socket listener
	fds[MAX_MIPS+1].fd = unixsocket;
	fds[MAX_MIPS+1].events = POLLIN;

	// Unix domain socket connections
	int uds = MAX_MIPS+2;
	for(; uds < MAX_MIPS+MAX_SOCKETS; uds++) {
		fds[uds].fd = -1;
		fds[uds].events = POLLIN | POLLOUT | POLLHUP;
	}

	// Enter loop
	while(1) {
		int rd = poll(fds, MAX_SOCKETS+MAX_MIPS, 0);
		size_t buflen = MIP_MAX_LEN+sizeof(struct eth_frame);
		char buf[buflen];

		// Check if error occured
		if(rd == -1) {
			perror("poll()");
			break;
		}

		// Check ethernet socket
		int ceths = 0;
		for(; ceths < nomips; ceths++) {
			if(fds[ceths].revents != 0) checketh(fds[ceths], mipaddrs[ceths]);
		}

		// Check unix socket
		if(fds[MAX_MIPS+1].revents & POLLIN) {
			// Incoming connection
			if(debug) fprintf(stderr, "MIPD: Incoming connection from ");
			memset(buf, 0, buflen);
			int tmp = accept(fds[MAX_MIPS+1].fd, NULL, NULL);
			recv(tmp, buf, buflen, 0);
			
			struct mipd_packet *id = (struct mipd_packet *)buf;
			if(id->dst_mip == RDID && fds[MAX_MIPS+2].fd == -1){
				// Routing daemon connected
				if(debug) fprintf(stderr, "Routing daemon\n");
				fds[MAX_MIPS+2].fd = tmp;

				size_t msgsz = sizeof(struct route_dg)+(nomips*sizeof(struct route_inf));
				struct route_dg *rd = malloc(msgsz);
				memset(rd, 0, msgsz);

				rd->local_mip = rd->src_mip = mipaddrs[0];
				rd->records_len = nomips;
				rd->mode = 1;

				int i = 0;
				for(; i < nomips; i++) {
					rd->records[i].cost = 31;
					rd->records[i].mip = mipaddrs[i];
				}

				sendus(msgsz, RDID, mipaddrs[0], (char *)rd);

			} else if(id->dst_mip == TPID && fds[MAX_MIPS+3].fd == -1) {
				// Transport daemon connected
				if(debug) fprintf(stderr, "Transport daemon\n");
				fds[MAX_MIPS+3].fd = tmp;

			} else {
				if(debug) fprintf(stderr, "Unknown. Rejected.\n");
				close(tmp);
			}
		}

		if(fds[MAX_MIPS+2].fd == -1 && ushasmessage(RDID)) {
			while(ushasmessage(RDID)) {
				char *tmp;
				size_t msgsize;
				usgetmessage(RDID, &msgsize, &tmp);
				free(tmp);
			}
		}

		if(fds[MAX_MIPS+3].fd == -1 && ushasmessage(TPID)) {
			while(ushasmessage(TPID)) {
				char *tmp;
				size_t msgsize;
				usgetmessage(TPID, &msgsize, &tmp);
				free(tmp);
			}
		}

		if(fds[MAX_MIPS+2].revents != 0) checkus(&fds[MAX_MIPS+2], RDID);
		if(fds[MAX_MIPS+3].revents != 0) checkus(&fds[MAX_MIPS+3], TPID);

		// Cleanup lists
		rinsearplist();

	}

	int i = 0;
	for(; i < nomips; i++)
		close(fds[i].fd);

	close(unixsocket);
	if(fds[MAX_MIPS+2].fd != -1) close(fds[MAX_MIPS+2].fd);
	if(fds[MAX_MIPS+3].fd != -1) close(fds[MAX_MIPS+3].fd);

	clearmip();
	freemap(NULL);
	clearus();

	return 0;
}

/**
 * Sets up ethernet sockets of type SOCK_RAW for all MIP addresses
 * configured for the MIP daemon. If the computer runs out of ethernet
 * interfaces before all MIP addresses are mapped to an interface, the
 * daemon simply discards the superfluous MIP addresses.
 * @param  fds Poll FD set to add the file descriptors to
 * @return     Returns the number of sockets created, or a negative number on error
 */
int setupethsocket(struct pollfd *fds) {
	struct ifaddrs *ifa, *current;
	getifaddrs(&ifa);
	current = ifa;
	int fdscreated = 0;

	while(current != NULL) {
		// Check that there are more MIP Addresses to take from
		if(fdscreated == nomips) break;

		// Check that current is an ethernet address
		if(current->ifa_addr->sa_family != AF_PACKET) {
			current = current->ifa_next;
			continue;
		}

		// Check that we are not assigning an address to loopback
		if(debug) fprintf(stderr, "MIPD: Interface %s ", current->ifa_name);
		if(strcmp(current->ifa_name, "lo") == 0){
			if(debug) fprintf(stderr, "discarded\n");
			current = current->ifa_next;
			continue;
		} else if(debug) fprintf(stderr, "assigned MIP address %d\n", mipaddrs[fdscreated]);

		// Create ifreq for ioctl
		struct ifreq ifr;
		memset(&ifr, 0, sizeof(ifr));
		strcpy(ifr.ifr_name, current->ifa_name);

		// Create socket
		int sock = socket(AF_PACKET, SOCK_RAW, ETH_P_MIP);
		if(sock == -1) {
			fprintf(stderr, "MIPD: Error creating socket for MIP %d ", mipaddrs[fdscreated]);
			perror("");
			break;
		}
		
		struct sockaddr_ll ethaddr;
		memset(&ethaddr, 0, sizeof(struct sockaddr_ll));

		// Get interface index
		if(ioctl(sock, SIOCGIFINDEX, &ifr) == -1) {
			fprintf(stderr, "MIPD: FError finding interface index for %s ", current->ifa_name);
			perror("");
			break;
		}

		ethaddr.sll_ifindex = ifr.ifr_ifindex;

		// Find MAC address
		if(ioctl(sock, SIOCGIFHWADDR, &ifr) == -1) {
			fprintf(stderr, "MIPD: Error finding interface address for %s ", current->ifa_name);
			perror("");
			break;
		}

		ethaddr.sll_family = AF_PACKET;
		ethaddr.sll_protocol = htons(ETH_P_MIP);

		if(bind(sock, (struct sockaddr *)&ethaddr, sizeof(struct sockaddr_ll)) == -1) {
			fprintf(stderr, "MIPD: Error binding socket to interface %s ", current->ifa_name);
			perror("");
			break;
		}

		addmapping((char *)ifr.ifr_hwaddr.sa_data, mipaddrs[fdscreated]);
		fds[fdscreated].fd = sock;
		fds[fdscreated].events = POLLIN | POLLOUT;

		fdscreated++;
		current = current->ifa_next;
	}

	freeifaddrs(ifa);

	if(fdscreated < nomips){
		int i = fdscreated;
		if(debug) fprintf(stderr, "MIPD: No more interfaces available. Discarding mips: ");
		for(; i < nomips; i++) {
			if(debug) fprintf(stderr, "%d ", mipaddrs[i]);
			mipaddrs[i] = -1;
			fds[i].fd = -1;
		}
		if(debug) fprintf(stderr, "\n");
	}

	return fdscreated == 0 ? -1 : fdscreated;

}

/**
 * Sets up a unix domain socket for communication between applications
 * and the MIP daemon.
 * @return FD of the socket, or negative value on error
 *            -1 Error creating the socket
 *            -2 Error binding the socket
 *            -3 Error listening on socket
 */
int setupunixsocket() {
	int unixsocket = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if(unixsocket == -1) {
		perror("MIPD: Creating unix socket");
		return -1;
	}

	// Create socket name
	char *upath = malloc(UNIX_PATH_MAX);
	memset(upath, 0, UNIX_PATH_MAX);

	// Name will be "socket" followed by the first configured MIP address
	sprintf(upath, "socket%d", mipaddrs[0]);

	struct sockaddr_un unaddr;
	memset(&unaddr, 0, sizeof(struct sockaddr_un));
	unaddr.sun_family = AF_UNIX;
	strcpy(unaddr.sun_path, upath);
	free(upath);

	// Delete socket if exist
	unlink(unaddr.sun_path);

	// Bind and listen
	if(bind(unixsocket, (struct sockaddr *) &unaddr, sizeof(struct sockaddr_un)) == -1) {
		perror("MIPD: Binding unix socket");
		return -2;
	}

	if(listen(unixsocket, UNIX_MAX_QUEUE) == -1) {
		perror("MIPD: Setting unix socket to listener");
		return -3;
	}

	return unixsocket;
}

/**
 * Checks the arguments given in the command line
 * @param  argc Number of arguments given
 * @param  argv The arguments
 * @return      Negative value on error, number of MIP addresses on success
 */
int checkargs(int argc, char *argv[]) {
	int i = 0;
	for(; i < argc-1; i++) {
		if(strcmp(argv[i+1], "-d") == 0) {
			debug = 1;
			return i <= MAX_MIPS ? i : MAX_MIPS;
		} else if(i > MAX_MIPS-1){
			fprintf(stderr, "%s: note: Max %d MIP addresses allowed. Ignoring %s\n", argv[0], MAX_MIPS, argv[i+1]);
			continue;
		} else {
			mipaddrs[i] = atoi(argv[i+1]);
			if((mipaddrs[i] == 0 && strcmp(argv[i+1], "0") != 0) || strncmp(argv[i+1], "-", 1) == 0) {
				fprintf(stderr, "%s: error: MIP Addresses must be integers between 0 and 255. %s is invalid\n", argv[0], argv[i+1]);
				fprintf(stderr, "%s: usage: %s [MIP addresses] [-d]\n", argv[0], argv[0]);
				return -1;
			}
		}
	}

	return i <= MAX_MIPS ? i : MAX_MIPS;

}

/**
 * Prints a hardware address in hex notation
 * @param addr Address to print
 */
void printhwaddr(char *addr) {
	int len = 6;
	int i = 0;
	for(; i < len; i++) {
		fprintf(stderr, "%02X", (unsigned char)addr[i]);
		if(i != len-1) fprintf(stderr, ":");
	}
}

/**
 * Checks whether given MIP address is a local MIP or not
 * @param  check MIP to be checked
 * @return       1 if yes, 0 if no
 */
int islocalmip(uint8_t check) {
	int i = 0;
	for(; i < nomips; i++) {
		if(mipaddrs[i] == check) return 1;
	}

	return 0;
}