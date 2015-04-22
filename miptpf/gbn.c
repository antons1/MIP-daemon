#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

#include "miptp.h"
#include "miptpproto.h"
#include "packetlist.h"
#include "applist.h"
#include "tpproto.h"
#include "../mipdf/mipdproto.h"

void recvApp(struct miptp_packet *, struct applist *);
void recvMip(struct mipd_packet *);
void recvAck(struct tp_packet *);
void recvData(struct tp_packet *);
int hasSendData(struct applist *);
int hasRecvData(struct applist *);
void getAppPacket(struct miptp_packet **, struct applist *);
void getMipPacket(struct mipd_packet **, struct applist *);
void resetWindow(struct applist *);

void recvApp(struct miptp_packet *recvd, struct applist *src) {
	size_t msglen = recvd->content_len-sizeof(struct miptp_packet);

	struct tp_packet *tpp;
	uint8_t pl = msglen%4;

	tpCreatepacket(pl, recvd->dst_port, src->sendinfo->nextAddSeqno++, msglen, recvd->content, &tpp);
	uint8_t totlen = sizeof(struct tp_packet)+msglen+pl;

	addPacket(tpp, totlen, src->sendinfo->sendQueue);
}

void recvMip(struct mipd_packet *recvd) {
	size_t msglen = recvd->content_len-sizeof(struct mipd_packet);

	struct tp_packet *tpp = malloc(msglen);
	memset(tpp, 0, msglen);
	memcpy(tpp, recvd->content, msglen);

	if(msglen == sizeof(struct tp_packet)) recvAck(tpp);
	else recvData(tpp);
}

void recvAck(struct tp_packet *recvd) {
	struct applist *app;
	getApp(recvd->port, &app);
	app->sendinfo->lastAckTime = time(NULL);
	app->sendinfo->lastAckSeqno = recvd->seqno;

	if(app->sendinfo->lastAckSeqno < app->sendinfo->nextSendSeqno){
		resetWindow(app);
		app->sendinfo->nextSendSeqno = app->sendinfo->lastAckSeqno;
	}
}

void recvData(struct tp_packet *recvd) {
	struct applist *app;
	getApp(recvd->port, &app);

	if(recvd->seqno == app->recvinfo->nextRecvSeqno) {
		// Accept packet
	}

	// Send ACK
}

int hasSendData(struct applist *src) {
	return 0;
}

int hasRecvData(struct applist *src) {
	return 0;
}

void getAppPacket(struct miptp_packet **ret, struct applist *src) {

}

void getMipPacket(struct mipd_packet **ret, struct applist *src) {

}

void resetWindow(struct applist *chng) {
	int chngAmnt = (chng->sendinfo->nextSendSeqno)-(chng->sendinfo->lastAckSeqno);

	int i = 0;
	int windowPos = chng->sendinfo->nextWindowPos;
	for(; i < chngAmnt; i++) {
		windowPos--;
		if(windowPos < 0) windowPos = WINDOW_SIZE;
	}

	chng->sendinfo->nextWindowPos = windowPos;
}