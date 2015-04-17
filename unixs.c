#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "mip.h"
#include "messagelist.h"
#include "mipd.h"
#include "rd.h"

#define ROUTETIMEOUT 2

struct us_frame {
	uint8_t src_addr:8;
	uint8_t dst_addr:8;
	uint8_t mode:1;
	uint8_t padding:7;
	char content[0];
} __attribute__ ((packed));

struct r_list {
	uint8_t fdst;
	char *msg;
	struct r_list *next;
	time_t sent;
};

struct mllist *mlist;
struct r_list *rlist;

void readus(uint8_t, char *);
void sendus(char *);
int ushasmessage(uint8_t);
int usgetmessage(uint8_t, char **);
void readrd(char *);
void sendrd(uint8_t, uint8_t, char *);

/**
 * Sends a message to IPC socket with given id
 * @param id  Where to send the message
 * @param msg Message to be sent
 */
void readus(uint8_t id, char *msg) {
	if(debug) fprintf(stderr, "MIPD: readus(%d, %p)\n", id, msg);
	struct us_frame *packet = (struct us_frame *)msg;

	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *check;
	getmlist(id, mlist, &check);

	if(debug) {
		fprintf(stderr, "MIPD: IPC packet to id %d | ", id);
		fprintf(stderr, "Mode: %d | Reserved: %d\n", packet->mode, packet->padding);
	}

	addmessage(msg, check);
}

/**
 * Reads a message from IPC and delegates it to correct functions
 * @param msg Recieved message
 */
void sendus(char *msg) {
	if(debug) fprintf(stderr, "MIPD: sendus(%p)\n", msg);
	struct us_frame *packet = (struct us_frame *)msg;

	if(debug) printf("MIPD: Mode: %d | To: %d | From: %d | Padding: %d | Content: %s\n", packet->mode, packet->dst_addr, packet->src_addr, packet->padding, packet->content);

	if(packet->padding == 0) sendrd(packet->src_addr, packet->dst_addr, msg);
	else readrd(msg);
}

/**
 * Checks if there are any messages in the message queue for given ID
 * @param  id Id to be checked
 * @return    1 if yes, 0 if no
 */
int ushasmessage(uint8_t id) {
	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *check;
	getmlist(id, mlist, &check);
	return hasmessage(check);
}

/**
 * Gets a message from queue for given ID
 * @param  id  ID to get message from
 * @param  msg Where message is stored
 * @return     1 if yes, 0 if no
 */
int usgetmessage(uint8_t id, char **msg) {
	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *check;
	getmlist(id, mlist, &check);
	return getmessage(msg, check);
}

/**
 * Sends a route request to local RD, stores data in a queue
 * @param src Source interface where message should be sent from
 * @param dst Destination MIP where message should be sent to
 * @param msg Message to be sent
 */
void sendrd(uint8_t src, uint8_t dst, char *msg) {
	if(debug) fprintf(stderr, "MIPD: sendrd(%d, %d, %p)\n", src, dst, msg);
	struct route_dg *req = malloc(sizeof(struct route_dg)+sizeof(struct route_inf));
	memset(req, 0, sizeof(struct route_dg)+sizeof(struct route_inf));

	req->mode = 0;	// Requestd
	req->src_mip = req->local_mip = src;	// When equal, it means route request
	req->records_len = 1;

	struct route_inf *rinf = &(req->records[0]);
	rinf->mip = dst;
	rinf->cost = TTL_MAX+1;

	readus(RDID, (char *)req);

	if(rlist == NULL) {
		rlist = malloc(sizeof(struct r_list));
		memset(rlist, 0, sizeof(struct r_list));
		rlist->next = NULL;
	}

	struct r_list *tmp = malloc(sizeof(struct r_list));
	memset(tmp, 0, sizeof(struct r_list));
	tmp->next = NULL;
	tmp->msg = malloc(MIP_MAX_LEN-sizeof(struct mip_frame));
	memset(tmp->msg, 0, MIP_MAX_LEN-sizeof(struct mip_frame));
	memcpy(tmp->msg, msg, MIP_MAX_LEN-sizeof(struct mip_frame));
	tmp->fdst = dst;
	tmp->sent = time(NULL);

	tmp->next = rlist->next;
	rlist->next = tmp;
}

/**
 * Reads a message from the local RD and either sends waiting messages, or sends the
 * packet on to correct external RDs
 * @param msg Recieved message
 */
void readrd(char *msg) {
	if(debug) fprintf(stderr, "MIPD: readrd(%p)\n", msg);
	struct route_dg *rd = (struct route_dg *)msg;

	if(debug) {
		fprintf(stderr, "MIPD: Recieved route ");
		fprintf(stderr, "LCL: %d | SRC: %d | DST: %d\n", rd->local_mip, rd->src_mip, rd->records[0].mip);
	}

	// If src is local, this is for daemon
	if(islocalmip(rd->src_mip)) {
		// Send waiting routing packets
		struct r_list *rl = rlist;
		if(rl == NULL) return;

		while(rl->next != NULL) {
			if(rl->next->fdst == rd->records[0].mip) {
				if(debug) {
					struct us_frame *uf = (struct us_frame *)rl->next->msg;
					fprintf(stderr, "MIPD: Sending message: Mode: %d | Padding: %d | To: %d | From %d\n", uf->mode, uf->padding, uf->dst_addr, uf->src_addr);
				}
				if(rd->records[0].cost < 16) sendtransport(rd->src_mip, rd->local_mip, rd->records[0].mip, rl->next->msg);
				struct r_list *tmp = rl->next;
				rl->next = rl->next->next;
				free(tmp);
			}

			rl = rl->next;
			if(rl == NULL) break;
		}

	} else if(rd->src_mip != 0 && rd->local_mip != 0) {
		// Packet for specific RD
		sendroute(rd->local_mip, (int16_t)rd->src_mip, (int16_t)rd->src_mip, msg);
		
	} else {
		// Broadcast to all RDs
		int i = 0;
		for(; i < nomips; i++) {
			sendroute(mipaddrs[i], 255, 255, msg);
		}
		
	}

}

/**
 * Removes waiting messages that has expired
 */
void rinserdlist() {
	struct r_list *rl = rlist;

	if(rlist == NULL) return;

	while(rl->next != NULL) {
		if(rl->next->sent >= time(NULL)+ROUTETIMEOUT) {
			struct r_list *tmp = rl->next;
			rl->next = rl->next->next;
			free(tmp->msg);
			free(tmp);
		}

		rl = rl->next;
		if(rl == NULL) break;
	}

}

/**
 * Clears IPC queues
 */
void clearus() {
	clearmlist(mlist);
}