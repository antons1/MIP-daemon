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
	uint8_t nextWindowPos;
	time_t lastAckTime;
	uint32_t lastAckSeqno;
	struct packetlist readyQueue[WINDOW_SIZE];
	struct packetlist *sendQueue;
};

struct recvinfo {
	uint32_t nextRecvSeqno;
	struct packetlist *recvQueue;
};

struct applist {
	uint16_t port;
	uint8_t fdind;
	struct sendinfo *sendinfo;
	struct recvinfo *recvinfo;
	struct applist *next;
};

int getNextApp(struct applist **);
int getApp(uint16_t, struct applist **);
int addApp(uint16_t, uint8_t, struct applist **);
int rmApp(uint16_t);
void initroot();
void initdata();

static struct applist *root;
static struct applist *current;

int getNextApp(struct applist **ret) {
	initroot();
	if(current == NULL) {
		current = root;
	}

	current = current->next;
	*ret = current;

	return (*ret == NULL) ? 0 : 1;
}

int getApp(uint16_t port, struct applist **ret) {
	initroot();

	struct applist *srch = root;
	while(srch->next != NULL) {
		if(srch->next->port == port) {
			if(ret != NULL) *ret = srch->next;
			return 1;
		}

		srch = srch->next;
	}

	return 0;
}

int addApp(uint16_t port, uint8_t fdind, struct applist **ret) {
	initroot();

	struct applist *srch = root;
	while(srch->next != NULL) srch = srch->next;

	srch->next = malloc(sizeof(struct applist));
	srch = srch->next;
	memset(srch, 0, sizeof(struct applist));

	srch->port = port;
	srch->fdind = fdind;
	srch->next = NULL;

	initdata(srch);

	if(ret != NULL)	*ret = srch;

	return 1;
}

int rmApp(uint16_t port) {
	initroot();

	struct applist *srch = root;
	while(srch->next != NULL) {
		if(srch->next->port == port) {
			struct applist *tmp = srch->next;
			srch->next = tmp->next;

			if(current->port == tmp->port) {
				current = srch;
			}

			free(tmp);
			return 1;
		}

		srch = srch->next;
		if(srch == NULL) break;
	}

	return 0;
}

void initroot() {
	if(root == NULL) {
		root = malloc(sizeof(struct applist));
		memset(root, 0, sizeof(struct applist));
		root->next = NULL;

		initdata(root);
	}
}

void initdata(struct applist *init) {
	init->sendinfo = malloc(sizeof(struct sendinfo));
	init->recvinfo = malloc(sizeof(struct recvinfo));

	memset(init->sendinfo, 0, sizeof(struct sendinfo));
	memset(init->recvinfo, 0, sizeof(struct sendinfo));

	init->sendinfo->sendQueue = malloc(sizeof(struct applist));
	init->recvinfo->recvQueue = malloc(sizeof(struct applist));

	memset(init->sendinfo->sendQueue, 0, sizeof(struct applist));
	memset(init->recvinfo->recvQueue, 0, sizeof(struct applist));
}