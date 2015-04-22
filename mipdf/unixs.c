#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include "mip.h"
#include "messagelist.h"
#include "mipd.h"
#include "../rdf/rd.h"
#include "mipdproto.h"

#define ROUTETIMEOUT 2

struct r_list {
	uint8_t fdst;
	char *msg;
	time_t sent;
	size_t msglen;
	struct r_list *next;
};

struct mllist *mlist;
struct r_list *rlist;

void readus(uint8_t, char *);
void sendus(size_t, uint8_t, char *);
int ushasmessage(uint8_t);
int usgetmessage(uint8_t, size_t *, char **);
void readrd(char *, size_t);
void sendrd(uint8_t, uint8_t, size_t, char *);

/**
 * Sends a message to IPC socket with given id
 * @param id  Where to send the message
 * @param msg Message to be sent
 */
void readus(uint8_t id, char *msg) {
	if(debug) fprintf(stderr, "MIPD: readus(%d, %p)\n", id, msg);
	struct mipd_packet *packet = (struct mipd_packet *)msg;

	if(debug) fprintf(stderr, "MIPD: Destinaton: %d | Lenght: %d\n", packet->dst_mip, packet->content_len);

	if(!islocalmip(packet->dst_mip)) sendrd(mipaddrs[0], packet->dst_mip, packet->content_len, (char *)packet->content);
	else readrd((char *)packet->content, packet->content_len);
}

/**
 * Reads a message from IPC and delegates it to correct functions
 * @param msg Recieved message
 */
void sendus(size_t len, uint8_t id, char *msg) {
	if(debug) fprintf(stderr, "MIPD: sendus(%zu, %d, %p)\n", len, id, msg);
	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *check;
	getmlist(id, mlist, &check);

	if(debug) {
		fprintf(stderr, "MIPD: IPC packet to id %d\n", id);
	}

	struct mipd_packet *mp;
	mipdCreatepacket(mipaddrs[0], len, msg, &mp);

	addmessage((char *)mp, len+sizeof(struct mipd_packet), check);
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
int usgetmessage(uint8_t id, size_t *msglen, char **msg) {
	if(mlist == NULL) {
		mlist = malloc(sizeof(struct mllist));
		memset(mlist, 0, sizeof(struct mllist));
		mlist->next = NULL;
	}

	struct messagelist *check;
	getmlist(id, mlist, &check);

	return getmessage(msg, msglen, check);
}

/**
 * Sends a route request to local RD, stores data in a queue
 * @param src Source interface where message should be sent from
 * @param dst Destination MIP where message should be sent to
 * @param msg Message to be sent
 */
void sendrd(uint8_t src, uint8_t dst, size_t msglen, char *msg) {
	if(debug) fprintf(stderr, "MIPD: sendrd(%d, %d, %p)\n", src, dst, msg);
	struct route_dg *req = malloc(sizeof(struct route_dg)+sizeof(struct route_inf));
	memset(req, 0, sizeof(struct route_dg)+sizeof(struct route_inf));

	req->mode = 0;	// Requestd
	req->src_mip = req->local_mip = src;	// When equal, it means route request
	req->records_len = 1;

	struct route_inf *rinf = &(req->records[0]);
	rinf->mip = dst;
	rinf->cost = TTL_MAX+1;

	sendus(sizeof(struct route_dg)+sizeof(struct route_inf), RDID, (char *)req);

	if(rlist == NULL) {
		rlist = malloc(sizeof(struct r_list));
		memset(rlist, 0, sizeof(struct r_list));
		rlist->next = NULL;
	}

	struct r_list *tmp = malloc(sizeof(struct r_list));
	memset(tmp, 0, sizeof(struct r_list));
	tmp->next = NULL;
	tmp->msg = malloc(msglen);
	memset(tmp->msg, 0, msglen);
	memcpy(tmp->msg, msg, msglen);
	tmp->fdst = dst;
	tmp->sent = time(NULL);
	tmp->msglen = msglen;

	tmp->next = rlist->next;
	rlist->next = tmp;
}

/**
 * Reads a message from the local RD and either sends waiting messages, or sends the
 * packet on to correct external RDs
 * @param msg Recieved message
 */
void readrd(char *msg, size_t msglen) {
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
					struct route_dg *mp = (struct route_dg *)rl->next->msg;
					fprintf(stderr, "MIPD: Sending message: To %d, length %u\n", mp->local_mip, mp->records_len);
				}
				if(rd->records[0].cost < 16) sendtransport(rd->src_mip, rd->local_mip, rd->records[0].mip, rl->next->msglen, rl->next->msg);
				struct r_list *tmp = rl->next;
				rl->next = rl->next->next;
				free(tmp);
			}

			rl = rl->next;
			if(rl == NULL) break;
		}

	} else if(rd->src_mip != 0 && rd->local_mip != 0) {
		// Packet for specific RD
		sendroute(rd->local_mip, (int16_t)rd->src_mip, (int16_t)rd->src_mip, msglen, msg);
		
	} else {
		// Broadcast to all RDs
		int i = 0;
		for(; i < nomips; i++) {
			sendroute(mipaddrs[i], 255, 255, msglen, msg);
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