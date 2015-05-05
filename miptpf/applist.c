#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include "miptp.h"
#include "packetlist.h"

struct sendinfo {
	uint32_t nextSendSeqno;
	uint32_t nextAddSeqno;
	time_t lastAckTime;
	uint32_t seqBase;
	uint32_t seqMax;
	struct packetlist *sendQueue;
};

struct recvinfo {
	uint32_t nextRecvSeqno;
	struct packetlist *recvQueue;
	struct packetlist *ackQueue;
};

struct applist {
	uint16_t port;
	uint8_t fdind;
	time_t lastTimeout;
	uint8_t disconnected;
	struct sendinfo *sendinfo;
	struct recvinfo *recvinfo;
	struct applist *next;
};

int getApp(uint16_t, struct applist **, struct applist *);
int addApp(uint16_t, uint8_t, struct applist **, struct applist *);
int rmApp(uint16_t, struct applist *);
void initroot(struct applist **);
void initdata(struct applist *);
void freeAppList(struct applist *);

int getApp(uint16_t port, struct applist **ret, struct applist *srch) {
	if(debug) fprintf(stderr, "MIPTP: getApp(%d, (return parameter))\n", port);
	
	while(srch->next != NULL) {
		if(srch->next->port == port) {
			if(ret != NULL) *ret = srch->next;
			return 1;
		}

		srch = srch->next;
	}

	return 0;
}

int addApp(uint16_t port, uint8_t fdind, struct applist **ret, struct applist *srch) {
	if(debug) fprintf(stderr, "MIPTP: addApp(%d, %d,(Return parameter))\n", port, fdind);
	
	while(srch->next != NULL) srch = srch->next;

	srch->next = malloc(sizeof(struct applist));
	srch = srch->next;
	memset(srch, 0, sizeof(struct applist));

	srch->port = port;
	srch->fdind = fdind;
	srch->disconnected = 0;
	srch->next = NULL;

	initdata(srch);

	if(ret != NULL)	*ret = srch;

	return 1;
}

int rmApp(uint16_t port, struct applist *srch) {
	if(debug) fprintf(stderr, "MIPTP: rmApp(%d)\n", port);

	while(srch->next != NULL) {
		if(srch->next->port == port) {
			struct applist *tmp = srch->next;
			srch->next = tmp->next;
			
			freePacketList(tmp->sendinfo->sendQueue);
			freePacketList(tmp->recvinfo->recvQueue);
			freePacketList(tmp->recvinfo->ackQueue);
			free(tmp->sendinfo);
			free(tmp->recvinfo);
			free(tmp);
			return 1;
		}

		srch = srch->next;
		if(srch == NULL) break;
	}

	return 0;
}

void initroot(struct applist **root) {
	if(*root == NULL) {
		if(debug) fprintf(stderr, "MIPTP: initroot()\n");
		*root = malloc(sizeof(struct applist));
		memset(*root, 0, sizeof(struct applist));
		(*root)->next = NULL;

		initdata(*root);
	}
}

void initdata(struct applist *init) {
	if(debug) fprintf(stderr, "MIPTP: initdata(%p)\n", init);
	init->sendinfo = malloc(sizeof(struct sendinfo));
	init->recvinfo = malloc(sizeof(struct recvinfo));

	memset(init->sendinfo, 0, sizeof(struct sendinfo));
	memset(init->recvinfo, 0, sizeof(struct recvinfo));

	init->sendinfo->sendQueue = malloc(sizeof(struct packetlist));
	init->recvinfo->recvQueue = malloc(sizeof(struct packetlist));
	init->recvinfo->ackQueue = malloc(sizeof(struct packetlist));

	memset(init->sendinfo->sendQueue, 0, sizeof(struct packetlist));
	memset(init->recvinfo->recvQueue, 0, sizeof(struct packetlist));
	memset(init->recvinfo->ackQueue, 0, sizeof(struct packetlist));

	init->sendinfo->sendQueue->next = NULL;
	init->recvinfo->recvQueue->next = NULL;
	init->recvinfo->ackQueue->next = NULL;

	init->sendinfo->seqMax = WINDOW_SIZE-1;
}

void freeAppList(struct applist *al) {
	if(al == NULL) return;

	if(al->next != NULL) freeAppList(al->next);
	freePacketList(al->sendinfo->sendQueue);
	freePacketList(al->recvinfo->recvQueue);
	freePacketList(al->recvinfo->ackQueue);
	free(al->sendinfo);
	free(al->recvinfo);
	free(al);
}