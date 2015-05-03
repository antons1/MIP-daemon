// Includes
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <arpa/inet.h>

#include "mipd.h"
#include "mipmac.h"
#include "messagelist.h"

// Definitions
#define MIP_MAX_LEN 1500
#define ETH_P_MIP 65535

#define TRA_T 4
#define TRA_R 2
#define TRA_A 1
#define TRA_E 0
#define TTL_MAX 15

#define MIP_TIMEOUT 10

// Structs
// Ethernet frame struct
struct eth_frame {
	char dst_addr[6];
	char src_addr[6];
	char eth_proto[2];
	char content[0];
} __attribute__ ((packed));

// MIP frame struct 
struct mip_frame {
	uint8_t tra_bits:3;
	uint8_t ttl:4;
	uint16_t content_len:9;
	uint8_t src_addr:8;
	uint8_t dst_addr:8;
	char content[0];
} __attribute__ ((packed));

#include "unixs.h"
#include "../rdf/rd.h"

// Wait for ARP list
struct arp_list {
	struct arp_list *next;
	char *msg;
	time_t sent;
	size_t msglen;
	uint8_t dst_addr;
	uint8_t via_addr;
	uint8_t src_addr;
};

// List of messagelists (one for each configured MIP address)


static struct arp_list *alist;
static struct mllist *mlist;

// Read functions
int miphasmessage(uint8_t);
void readmip(char *, uint8_t);
int mipgetmessage(uint8_t, size_t *, char **);
void readtransport(struct mip_frame *);
void readarp(struct mip_frame *);
void readarpr(struct mip_frame *);
void readroute(struct mip_frame *, uint8_t);

// Send functions
void sendmip(uint8_t, uint8_t, uint8_t, uint8_t, size_t, char *);
void sendarp(uint8_t, uint8_t);
void sendarpr(uint8_t, uint8_t);
void sendtransport(uint8_t, uint8_t, uint8_t, size_t, char *);
void sendroute(uint8_t, int16_t, int16_t, size_t, char *);

/**
 * Checks whether a MIP message is queued to send
 * @return 1 if there is a waiting message, 0 if not
 */
int miphasmessage(uint8_t src) {
	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *check;
	getmlist(src, mlist, &check);
	return hasmessage(check);
}

/**
 * Takes a MIP message and places it in correct structs,
 * checking that the message is intended to this host and
 * delegates it to the appropriate function
 * @param msg Message that was delivered
 */
void readmip(char *msg, uint8_t src) {
	if(debug) fprintf(stderr, "MIPD: readmip(%p)\n", msg);
	struct eth_frame *eframe = (struct eth_frame *)msg;

	if(debug) {
		fprintf(stderr, "MIPD: Reading MIP message: ");
		fprintf(stderr, "From MAC ");
		printhwaddr(eframe->src_addr);
		fprintf(stderr, " to MAC ");
		printhwaddr(eframe->dst_addr);
		fprintf(stderr, "\n");
	}

	struct mip_frame *mframe = (struct mip_frame *)eframe->content;
	size_t msglen = ((mframe->content_len)*4)-sizeof(struct mip_frame);

	if(debug) fprintf(stderr, "MIPD: From MIP %d to MIP %d | ", mframe->src_addr, mframe->dst_addr);
	if(debug) fprintf(stderr, "Type: %d | TTL: %d | Length: %d\n", mframe->tra_bits, mframe->ttl, mframe->content_len);

	if(!islocalmip(mframe->dst_addr) && mframe->tra_bits != TRA_R) {
		if(debug) fprintf(stderr, "MIPD: Frame is not for this daemon, passing on\n");
		if(mframe->ttl != 0) {
			mframe->ttl -= 1;
			char *msg = malloc(msglen);
			memcpy(msg, mframe->content, msglen);
			sendrd(mframe->src_addr, mframe->dst_addr, msglen, msg);
		} else if(debug) fprintf(stderr, "MIPD: Frame had TTL 0, dropping...\n");

		return;
	}

	addmapping(eframe->src_addr, mframe->src_addr);

	switch(mframe->tra_bits) {
		case TRA_T:
		readtransport(mframe);
		break;

		case TRA_A:
		readarp(mframe);
		break;

		case TRA_E:
		readarpr(mframe);
		break;

		case TRA_R:
		readroute(mframe, src);
		break;

		default:
		break;
	}
}

/**
 * Gets the next message from the message queue
 * @param  msg Where the message is placed
 * @return     returns 0 if no message was available, else 1
 */
int mipgetmessage(uint8_t src, size_t *msglen, char **msg) {
	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *check;
	getmlist(src, mlist, &check);

	return getmessage(msg, msglen, check);
}

/**
 * Recieves a MIP transport frame, validates it and passes on data
 * @param frame Recieved MIP frame
 */
void readtransport(struct mip_frame *frame) {
	if(debug) fprintf(stderr, "MIPD: readtransport(%p)\n", frame);
	// Pass on to ping

	size_t msglen = ((frame->content_len)*4)-sizeof(struct mip_frame);
	if(debug) fprintf(stderr, "MIPD: RECIEVED LEN: %zd\n", msglen);
 	char *msg = malloc(msglen);
 	memset(msg, 0, msglen);
 	memcpy(msg, frame->content, msglen);

 	sendus(msglen, TPID, frame->src_addr, msg);

}

/**
 * Reads a MIP frame with TRA set to R.
 * @param frame Recieved frame
 * @param src   MIP address associated with interface message arrived on
 */
void readroute(struct mip_frame *frame, uint8_t src) {
	// Send route datagram to routing daemon
	if(debug) fprintf(stderr, "MIPD: readroute(%p, %d)\n", frame, src);
	size_t msglen = ((frame->content_len)*4)-sizeof(struct mip_frame);
	char *msg = malloc(msglen);
	memset(msg, 0, msglen);
	memcpy(msg, frame->content, msglen);

	struct route_dg *rd = (struct route_dg *)msg;
	if(rd->src_mip == 0) rd->src_mip = src;
	if(rd->local_mip == 0) rd->local_mip = frame->src_addr;

	if(debug) fprintf(stderr, "MIPD: Sending to RD: LCL %d | SRC %d | LEN %d | MOD %d\n", rd->local_mip, rd->src_mip, rd->records_len, rd->mode);
	
	sendus(msglen, RDID, mipaddrs[0], msg);
}

/**
 * Recieves a MIP ARP message, and responds with an ARP response
 * @param frame Recieved MIP frame
 */
void readarp(struct mip_frame *frame) {
	// Send ARPR
	if(debug) fprintf(stderr, "MIPD: readarp(%p)\n", frame);
	sendarpr(frame->dst_addr, frame->src_addr);
}

/**
 * Recieves a MIP ARP response, and stores the mapped addresses
 * @param frame Recieved MIP frame
 */
void readarpr(struct mip_frame *frame) {
	// Get messages from arp list and send them
	if(debug) fprintf(stderr, "MIPD: readarpr(%p)\n", frame);
	struct arp_list *current = alist;
	if(current == NULL) return;

	int i = 0;

	while(current->next != NULL) {
		if(current->next->dst_addr == frame->src_addr) {
			sendtransport(current->next->src_addr, current->next->via_addr, current->next->dst_addr, current->msglen, current->next->msg);
			struct arp_list *tmp = current->next;
			current->next = tmp->next;
			free(tmp->msg);
			free(tmp);
			i++;
		}

		current = current->next;
		if(current == NULL) break;
	}

	if(debug) fprintf(stderr, "MIPD: Sent %d messages to MIP %d\n", i, frame->src_addr);
}

/**
 * Takes a message and a destination MIP address, creates the necessary
 * frame structures, sends ARP message if necessary, and adds the message
 * to the queue of messages to send
 * @param dst Destination MIP address
 * @param msg Message to send
 */
void sendmip(uint8_t src, uint8_t via, uint8_t dst, uint8_t tra, size_t msglen, char *msg) {
	if(debug) fprintf(stderr, "MIPD: sendmip(%d, %d, %d, %d, %zu, %p)\n", src, via, dst, tra, msglen, msg);
	char dsthw[6];

	if(findhw(dsthw, via, NULL)) {
		// Address of recipient is known, put in list
		int miplen = (msglen+sizeof(struct mip_frame))/4;
		if((msglen+sizeof(struct mip_frame))%4 > 0) miplen++;
		struct mip_frame *mframe = malloc(miplen*4);
		memset(mframe, 0, miplen*4);

		mframe->src_addr = src;
		mframe->dst_addr = dst;
		mframe->tra_bits = tra;
		mframe->ttl = TTL_MAX;
		mframe->content_len = miplen;
		memcpy(mframe->content, msg, msglen);

		if(debug) {
			fprintf(stderr, "MIPD: MIP Frame: src: %d | dst: %d | tra: %d | ttl: %d | len: %d\n", mframe->src_addr, mframe->dst_addr, mframe->tra_bits, mframe->ttl, mframe->content_len);
		}

		struct eth_frame *eframe = malloc(sizeof(struct eth_frame)+(miplen*4));
		memset(eframe, 0, sizeof(struct eth_frame)+(miplen*4));

		eframe->eth_proto[1] = eframe->eth_proto[0] = 0xFF;
		findhw(eframe->src_addr, src, NULL);
		findhw(eframe->dst_addr, via, NULL);
		memcpy(eframe->content, mframe, miplen*4);
		free(mframe);

		if(debug) {
			fprintf(stderr, "MIPD: Ethernet frame: src: ");
			printhwaddr(eframe->src_addr);
			fprintf(stderr, " | dst: ");
			printhwaddr(eframe->dst_addr);
			fprintf(stderr, "\n");
		}

		if(mlist == NULL) {
			mlist = malloc(sizeof(struct mllist));
			memset(mlist, 0, sizeof(struct mllist));
			mlist->next = NULL;
		}

		struct messagelist *srclist;
		getmlist(src, mlist, &srclist);
		addmessage((char *)eframe, sizeof(struct eth_frame)+(miplen*4), srclist);
		free(eframe);

	} else {
		// Address of recipient is unknown, send arp
		if(alist == NULL) {
			alist = malloc(sizeof(struct arp_list));
			memset(alist, 0, sizeof(struct arp_list));
			alist->next = NULL;
		}

		if(debug) fprintf(stderr, "MIPD: Recipient unknown, sending ARP\n");
		struct arp_list *current = alist;
		while(current->next != NULL) current = current->next;
		current->next = malloc(sizeof(struct arp_list));
		memset(current->next, 0, sizeof(struct arp_list));
		current = current->next;
		current->next = NULL;

		current->msg = malloc(msglen);
		memcpy(current->msg, msg, msglen);
		current->dst_addr = dst;
		current->via_addr = via;
		current->src_addr = src;
		current->msglen = msglen;
		current->sent = time(NULL);
		sendarp(src, dst);

	}
}

/**
 * Sends the msg via sendmip with TRA set to T
 * @param src source MIP interface
 * @param via Next hop address
 * @param dst Final destination address
 * @param msg Message to be sent
 */
void sendtransport(uint8_t src, uint8_t via, uint8_t dst, size_t msglen, char *msg) {
	if(debug) fprintf(stderr, "MIPD: sendtransport(%d, %d, %d, %zu, %p)\n", src, via, dst, msglen, msg);
	sendmip(src, via, dst, TRA_T, msglen, msg);
}

/**
 * Sends msg via sendmip with TRA set to R. If via and dst is set to 255, message is broadcast on
 * src interface
 * @param src Source MIP interface
 * @param via Next hop address
 * @param dst Final destination address
 * @param msg Message to be sent
 */
void sendroute(uint8_t src, int16_t via, int16_t dst, size_t msglen, char *msg) {
	if(debug) fprintf(stderr, "MIPD: sendroute(%d, %d, %d, %zu, %p)\n", src, via, dst, msglen, msg);

	if(via == 255 && dst == 255) {
		// Broadcast on if src
		int miplen = (sizeof(struct mip_frame)+msglen)/4;
		if((sizeof(struct mip_frame)+msglen)%4) miplen++;

		struct mip_frame *mframe = malloc(miplen*4);
		memset(mframe, 0, miplen*4);
		mframe->tra_bits = TRA_R;
		mframe->src_addr = src;
		mframe->dst_addr = 255;
		mframe->ttl = TTL_MAX;
		mframe->content_len = miplen;
		memcpy(mframe->content, msg, msglen);

		struct eth_frame *eframe = malloc(sizeof(struct eth_frame)+(miplen*4));
		memset(eframe, 0, sizeof(struct eth_frame)+(miplen*4));
		memcpy(eframe->content, mframe, (miplen*4));
		findhw(eframe->src_addr, src, NULL);
		memset(eframe->dst_addr, 0xFF, 6);
		eframe->eth_proto[1] = eframe->eth_proto[0] = 0xFF;

		struct messagelist *srclist;
		getmlist(src, mlist, &srclist);
		addmessage((char *)eframe, sizeof(struct eth_frame)+miplen*4, srclist);

		free(mframe);
		free(eframe);

	} else sendmip(src, via, dst, TRA_R, msglen, msg);
}

/**
 * Send an ARP message for the given ethernet frame, adds the message to the
 * ARP waiting list
 * @param msg Ethernet frame to send
 */
void sendarp(uint8_t src, uint8_t dst) {
	if(debug) fprintf(stderr, "MIPD: sendarp(%d, %d)\n", src, dst);
	if(debug) fprintf(stderr, "MIPD: Sending ARP to identify %d\n", dst);
	int miplen = 1;
	struct mip_frame *mframe = malloc(miplen*4);
	memset(mframe, 0, miplen*4);
	mframe->src_addr = src;
	mframe->dst_addr = dst;
	mframe->tra_bits = TRA_A;
	mframe->ttl = TTL_MAX;
	mframe->content_len = miplen;

	struct eth_frame *eframe = malloc(sizeof(struct eth_frame)+(miplen*4));
	memset(eframe, 0, (sizeof(struct eth_frame)+(miplen*4)));
	eframe->eth_proto[1] = eframe->eth_proto[0] = 0xFF;
	memcpy(eframe->content, mframe, miplen*4);
	findhw(eframe->src_addr, src, NULL);
	memset(eframe->dst_addr, 0xFF, 6);

	if(debug) {
		fprintf(stderr, "MIPD: MIP frame: src: %d | dst: %d | tra: %d | ttl: %d | len: %d\n", mframe->src_addr, mframe->dst_addr, mframe->tra_bits, mframe->ttl, mframe->content_len);
		fprintf(stderr, "MIPD: Ethernet frame: src: ");
		printhwaddr(eframe->src_addr);
		fprintf(stderr, " | dst: ");
		printhwaddr(eframe->dst_addr);
		fprintf(stderr, "\n");
	}

	free(mframe);

	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *srclist;
	getmlist(src, mlist, &srclist);
	addmessage((char *)eframe, sizeof(struct eth_frame)+(miplen*4), srclist);
}

/**
 * Sends an ARP response to the given MIP address
 * @param dst MIP address to send APR response to
 */
void sendarpr(uint8_t src, uint8_t dst) {
	int miplen = 1;
	if(debug) fprintf(stderr, "MIPD: sendarpr(%d, %d)\n", src, dst);
	struct mip_frame *mframe = malloc(miplen*4);
	memset(mframe, 0, miplen*4);
	mframe->src_addr = src;
	mframe->dst_addr = dst;
	mframe->tra_bits = TRA_E;
	mframe->ttl = TTL_MAX;
	mframe->content_len = miplen;

	struct eth_frame *eframe = malloc(sizeof(struct eth_frame)+(miplen*4));
	memset(eframe, 0, (sizeof(struct eth_frame)+(miplen*4)));
	eframe->eth_proto[1] = eframe->eth_proto[0] = 0xFF;
	findhw(eframe->src_addr, src, NULL);
	findhw(eframe->dst_addr, dst, NULL);
	memcpy(eframe->content, mframe, miplen*4);

	if(debug) {
		fprintf(stderr, "MIPD: MIP frame: src: %d | dst: %d | tra: %d | ttl: %d | len: %d\n", mframe->src_addr, mframe->dst_addr, mframe->tra_bits, mframe->ttl, mframe->content_len);
		fprintf(stderr, "MIPD: Ethernet frame: src: ");
		printhwaddr(eframe->src_addr);
		fprintf(stderr, " | dst: ");
		printhwaddr(eframe->dst_addr);
		fprintf(stderr, "\n");
	}

	free(mframe);

	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *srclist;
	getmlist(src, mlist, &srclist);
	addmessage((char *)eframe, sizeof(struct eth_frame)+(miplen*4), srclist);
}

/**
 * Clears the ARP list
 * @param root Used in recursion, should always be NULL
 */
void clearalist(struct arp_list *root) {
	struct arp_list *next;
	if(root == NULL) next = alist;
	else next = root;

	if(next == NULL) return;

	if(next->next != NULL) clearalist(next->next);
	free(next->msg);
	free(next);
}

/**
 * Releases all resources bound by MIP protocol
 */
void clearmip() {
	clearmlist(mlist);
	clearalist(NULL);
}

/**
 * Removes messages waiting for ARP response that have timed out
 */
void rinsearplist() {
	if(alist == NULL) {
		alist = malloc(sizeof(struct arp_list));
		memset(alist, 0, sizeof(struct arp_list));
		alist->next = NULL;
	}
	struct arp_list *current = alist;

	if(current == NULL) return;

	time_t curtime = time(NULL);

	while(current->next != NULL) {
		if((current->next->sent+MIP_TIMEOUT) < curtime) {
			struct arp_list *tmp = current->next;
			current->next = tmp->next;
			free(tmp->msg);
			free(tmp);
		}

		current = current->next;
		if(current == NULL) break;
	}
}