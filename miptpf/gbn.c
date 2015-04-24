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
void recvData(struct tp_packet *, uint16_t, uint8_t);
int hasSendData(struct applist *);
int hasRecvData(struct applist *);
void sendAck(struct applist *, uint16_t, uint8_t);
void getAppPacket(struct miptp_packet **, struct applist *);
void getMipPacket(struct mipd_packet **, struct applist *);

void recvApp(struct miptp_packet *recvd, struct applist *src) {
	if(debug) fprintf(stderr, "MIPTP: recvApp(%p, %p)\n", recvd, src);
	size_t msglen = recvd->content_len-sizeof(struct miptp_packet);

	struct tp_packet *tpp;
	uint8_t pl = msglen%4;

	tpCreatepacket(pl, recvd->dst_port, src->sendinfo->nextAddSeqno++, msglen, recvd->content, &tpp);
	uint8_t totlen = sizeof(struct tp_packet)+msglen+pl;

	addPacket(tpp, recvd->dst_mip, totlen, src->sendinfo->sendQueue);
}

void recvMip(struct mipd_packet *recvd) {
	if(debug) fprintf(stderr, "MIPTP: recvMip(%p)\n", recvd);
	size_t msglen = recvd->content_len-sizeof(struct mipd_packet);

	struct tp_packet *tpp = malloc(msglen);
	memset(tpp, 0, msglen);
	memcpy(tpp, recvd->content, msglen);

	if(msglen == sizeof(struct tp_packet)) recvAck(tpp);
	else recvData(tpp, msglen, recvd->dst_mip);
}

void recvAck(struct tp_packet *recvd) {
	if(debug) fprintf(stderr, "MIPTP: recvAck(%p)\n", recvd);
	struct applist *app;
	getApp(recvd->port, &app);
	app->sendinfo->lastAckSeqno = recvd->seqno;
	app->sendinfo->lastAckTime = time(NULL);

}

void recvData(struct tp_packet *recvd, uint16_t datalen, uint8_t srcmip) {
	if(debug) fprintf(stderr, "MIPTP: recvData(%p, %d, %d)\n", recvd, datalen, srcmip);
	struct applist *app;
	getApp(recvd->port, &app);

	if(recvd->seqno == app->recvinfo->nextRecvSeqno) {
		// Accept packet
		app->recvinfo->nextRecvSeqno++;
		addPacket(recvd, srcmip, datalen, app->recvinfo->recvQueue);
	}

	sendAck(app, recvd->port, srcmip);
}

int hasSendData(struct applist *src) {
	if(debug) fprintf(stderr, "MIPTP: asSendData(%p)\n", src);
	if(src->sendinfo->sendQueue->next != NULL) return 1;
	else return 0;
}

int hasRecvData(struct applist *src) {
	if(debug) fprintf(stderr, "MIPTP: hasRecvData(%p)\n", src);
	if(src->recvinfo->recvQueue->next != NULL) return 1;
	else return 0;
}

void sendAck(struct applist *src, uint16_t port, uint8_t srcmip) {
	if(debug) fprintf(stderr, "MIPTP: sendAck(%p, %d, %d)\n", src, port, srcmip);
	struct tp_packet *tpp = malloc(sizeof(struct tp_packet));
	memset(tpp, 0, sizeof(struct tp_packet));

	tpp->seqno = src->recvinfo->nextRecvSeqno;
	tpp->port = port;
	tpp->pl_bits = 0;

	addPacket(tpp, srcmip, sizeof(struct tp_packet), src->sendinfo->sendQueue);
}

void getAppPacket(struct miptp_packet **ret, struct applist *src) {
	if(debug) fprintf(stderr, "MIPTP: getAppPacket(%p (%p), %p)\n", *ret, ret, src);
	struct packetlist *toget;
	getNextPacket(&toget, src->recvinfo->recvQueue);
	removeToSeqno(toget->data->seqno+1, src->recvinfo->recvQueue);

	struct miptp_packet *result;
	miptpCreatepacket(lmip, toget->data->port, toget->datalen-sizeof(struct tp_packet), toget->data->content, &result);
	*ret = result;
}

void getMipPacket(struct mipd_packet **ret, struct applist *src) {
	if(debug) fprintf(stderr, "MIPTP: getMipPacket(%p (%p), %p)\n", *ret, ret, src);
	struct tp_packet *tpp = NULL;
	size_t msglen;
	uint8_t dstmip;

	if(hasSendData(src) && src->sendinfo->nextSendSeqno == 0) {
		// This is a recieving port, all messages are ACKs
		struct packetlist *pct = NULL;
		getNextPacket(&pct, src->sendinfo->sendQueue);
		if(pct != NULL) {
			removeToSeqno(pct->data->seqno, src->sendinfo->sendQueue);
			tpp = pct->data;
			msglen = pct->datalen;
			dstmip = pct->dst_mip;
		}
	} else {
		// This is a sending port, check seqnos
		if(src->sendinfo->lastAckSeqno == src->sendinfo->nextSendSeqno) {
			// Ack has been recieved for last sent message
			removeToSeqno(src->sendinfo->nextSendSeqno, src->sendinfo->sendQueue);
		} else if((time(NULL)-src->sendinfo->lastAckTime) > timeout) {
			// Timeout without new ack, go back to previous ack
			removeToSeqno(src->sendinfo->lastAckSeqno, src->sendinfo->sendQueue);
			src->sendinfo->nextSendSeqno = src->sendinfo->lastAckSeqno;
		}

		if(((src->sendinfo->nextSendSeqno)-(src->sendinfo->lastAckSeqno)) < WINDOW_SIZE) {
			// Window size is not reached
			struct packetlist *pct = NULL;
			getPacket(src->sendinfo->nextSendSeqno, &pct, src->sendinfo->sendQueue);
			if(pct != NULL) {
				tpp = pct->data;
				msglen = pct->datalen;
				dstmip = pct->dst_mip;
			}
		}
	}

	struct mipd_packet *mdp = NULL;
	if(tpp != NULL) mipdCreatepacket(dstmip, msglen, (char *)tpp, &mdp);
	*ret = mdp;
}