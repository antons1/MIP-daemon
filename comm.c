#include <stdio.h>
#include <poll.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

#include "mip.h"
#include "mipd.h"
#include "unixs.h"

/**
 * Checks given ethernet socket file descriptor and sends/recieves messages from it,
 * then delegate them to correct functions
 * @param fd  pollfd struct containing fd and events for fd
 * @param src MIP address associated with given interface
 */
void checketh(struct pollfd fd, uint8_t src) {
	if(fd.revents & POLLIN) {
		// Data incoming on ethernet socket
		if(debug) fprintf(stderr, "MIPD: Recieving data on ethernet socket, MIP %d\n", src);
		char buf[MIP_MAX_LEN];
		ssize_t rb = recv(fd.fd, buf, MIP_MAX_LEN, 0);
		if(rb == -1) perror("MIPD: Error reading ethernet socket");
		else {
			readmip(buf, src);
		}
	}
	
	if((fd.revents & POLLOUT) && miphasmessage(src)) {
	// Data can be written, go here if data should be written
	if(debug) fprintf(stderr, "MIPD: Sending MIP message on ethernet socket, MIP %d\n", src);
		while(miphasmessage(src)) {
			char *sbuf;
			mipgetmessage(src, &sbuf);
			if(debug) {
				fprintf(stderr, "MIPD: Message: ");
				struct eth_frame *eframe = (struct eth_frame *)sbuf;
				fprintf(stderr, "To HW ");
				printhwaddr(eframe->dst_addr);
				fprintf(stderr, " from HW ");
				printhwaddr(eframe->src_addr);
				fprintf(stderr, " | ");
				struct mip_frame *mframe = (struct mip_frame *)eframe->content;
				fprintf(stderr, "To MIP %d from MIP %d\n", mframe->dst_addr, mframe->src_addr);
			}
		
			ssize_t sb = send(fd.fd, sbuf, MIP_MAX_LEN, 0);
			if(sb == -1) {
				perror("MIPD: Error sending ethernet frame");
			}
			
			free(sbuf);
		}
	}
}

/**
 * Chekcks given IPC fd, sends and recieves data on it, and delegates data to
 * correct functions
 * @param fd pollfd struct containing fd and returned events for fd
 * @param id IPC id of fd, as defined in PINCID, PINGSID and RDID
 */
void checkus(struct pollfd *fd, uint8_t id) {
	char buf[MIP_MAX_LEN];

	if(fd->revents & POLLHUP) {
		// PING client closed connection
		if(debug) fprintf(stderr, "MIPD: IPC client (%d) closed connection\n", id);
		close(fd->fd);
		fd->fd = -1;
		fd->revents = 0;
	}

	if(fd->revents & POLLIN) {
		// Client sends message
		if(debug) fprintf(stderr, "MIPD: Message from IPC client (%d)\n", id);
		memset(buf, 0, MIP_MAX_LEN);
		ssize_t rb = recv(fd->fd, buf, MIP_MAX_LEN, 0);
		if(rb == -1) perror("MIPD: Error reading IPC client");

		sendus(buf);
	}

	if((fd->revents & POLLOUT) && ushasmessage(id)) {
		// Response can be sent, go here if response should be sent
		if(debug) fprintf(stderr, "MIPD: Sending to IPC client(%d)\n", id);
		while(ushasmessage(id)) {
			char *sbuf;
			if(debug) fprintf(stderr, "MIPD: Getting messagelist for ID %d\n", id);
			usgetmessage(id, &sbuf);
			if(send(fd->fd, sbuf, MIP_MAX_LEN, 0) == -1) {
				perror("MIPD: Error sending message to IPC client");
			}
			free(sbuf);
		}
	}

}