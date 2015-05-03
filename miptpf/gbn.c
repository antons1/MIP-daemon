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
int hasAckData(struct applist *);
void sendAck(struct applist *, uint16_t, uint8_t);
void getAppPacket(struct miptp_packet **, struct applist *);
void getMipPacket(struct mipd_packet **, struct applist *);
void getAckPacket(struct mipd_packet **, struct applist *);
int doneSending(struct applist *);
int timedout(struct applist *);

void recvApp(struct miptp_packet *recvd, struct applist *src) {
	if(debug) fprintf(stderr, "MIPTP: recvApp(%p, %p)\n", recvd, src);
	uint16_t msglen = recvd->content_len;

	if(debug) fprintf(stderr, "MIPTP: Recieved length is %d (%d)\n", msglen, recvd->content_len);

	struct tp_packet *tpp;
	uint8_t pl = 4-(msglen%4);
	if(pl == 4) pl = 0;

	if(src->sendinfo->nextAddSeqno == 0) src->sendinfo->lastAckTime = time(NULL);

	tpCreatepacket(pl, recvd->dst_port, src->sendinfo->nextAddSeqno++, msglen, recvd->content, &tpp);
	uint16_t totlen = sizeof(struct tp_packet)+msglen+pl;

	addPacket(tpp, recvd->dst_mip, totlen, src->sendinfo->sendQueue);
}

void recvMip(struct mipd_packet *recvd) {
	if(debug) fprintf(stderr, "MIPTP: recvMip(%p)\n", recvd);
	uint16_t msglen = recvd->content_len;

	fprintf(stderr, "MIPTP: To port %d\n", ((struct tp_packet *)recvd->content)->port);

	struct tp_packet *tpp = malloc(msglen);
	memset(tpp, 0, msglen);
	memcpy(tpp, recvd->content, msglen);

	if(msglen == sizeof(struct tp_packet)) recvAck(tpp);
	else recvData(tpp, msglen, recvd->dst_mip);
}

void recvAck(struct tp_packet *recvd) {
	if(debug) fprintf(stderr, "MIPTP: recvAck(%p)\n", recvd);
	struct applist *app = NULL;
	getApp(recvd->port, &app);

	if(debug) fprintf(stderr, "MIPTP: ACK to port %d, app is %p\n", recvd->port, app);

	if(app == NULL) return;

	app->sendinfo->lastAckSeqno = recvd->seqno;
	if(app->sendinfo->lastAckSeqno < app->sendinfo->nextSendSeqno)
		app->sendinfo->nextSendSeqno = app->sendinfo->lastAckSeqno;

	if(debug) fprintf(stderr, "MIPTP: Recieved ACK, requesting seqno %d\n", app->sendinfo->lastAckSeqno);
	app->sendinfo->lastAckTime = time(NULL);
	if(app->lastTimeout != 0) app->lastTimeout = 0;

	free(recvd);

}

void recvData(struct tp_packet *recvd, uint16_t datalen, uint8_t srcmip) {
	if(debug) fprintf(stderr, "MIPTP: recvData(%p, %d, %d)\n", recvd, datalen, srcmip);
	struct applist *app = NULL;
	getApp(recvd->port, &app);

	if(app == NULL) return;

	if(recvd->seqno == app->recvinfo->nextRecvSeqno) {
		// Accept packet
		app->recvinfo->nextRecvSeqno++;
		addPacket(recvd, srcmip, datalen, app->recvinfo->recvQueue);
	}

	if(app->lastTimeout != 0) app->lastTimeout = 0;

	sendAck(app, recvd->port, srcmip);
}

int hasSendData(struct applist *src) {
	//if(debug) fprintf(stderr, "MIPTP: hasSendData(%p)\n", src);
	if(containsSeqno(src->sendinfo->nextSendSeqno, src->sendinfo->sendQueue) &&
		(((src->sendinfo->nextSendSeqno)-(src->sendinfo->lastAckSeqno)) < WINDOW_SIZE) && 
		(src->sendinfo->nextSendSeqno < src->sendinfo->nextAddSeqno)) {
		return 1;
	}
	else return 0;
}

int hasRecvData(struct applist *src) {
	//if(debug) fprintf(stderr, "MIPTP: hasRecvData(%p)\n", src);
	if(src->recvinfo->recvQueue->next != NULL) return 1;
	else return 0;
}

int hasAckData(struct applist *src) {
	if(src->recvinfo->ackQueue->next != NULL) return 1;
	else return 0;
}

void sendAck(struct applist *src, uint16_t port, uint8_t srcmip) {
	if(debug) fprintf(stderr, "MIPTP: sendAck(%p, %d, %d)\n", src, port, srcmip);
	struct tp_packet *tpp = malloc(sizeof(struct tp_packet));
	memset(tpp, 0, sizeof(struct tp_packet));

	tpp->seqno = src->recvinfo->nextRecvSeqno;
	tpp->port = port;
	tpp->pl_bits = 0;

	if(debug) fprintf(stderr, "MIPTP: Sent ACK, requesting seqno %d\n", tpp->seqno);

	addPacket(tpp, srcmip, sizeof(struct tp_packet), src->recvinfo->ackQueue);
}

void getAppPacket(struct miptp_packet **ret, struct applist *src) {
	//if(debug) fprintf(stderr, "MIPTP: getAppPacket(%p (%p), %p)\n", *ret, ret, src);
	struct packetlist *toget;
	getNextPacket(&toget, src->recvinfo->recvQueue);

	struct miptp_packet *result;
	miptpCreatepacket(lmip, toget->data->port, toget->datalen-sizeof(struct tp_packet)-toget->data->pl_bits, toget->data->content, &result);
	*ret = result;
	removeToSeqno(toget->data->seqno+1, src->recvinfo->recvQueue);
}

void getMipPacket(struct mipd_packet **ret, struct applist *src) {
	if(debug) fprintf(stderr, "MIPTP: getMipPacket()\n");
	struct tp_packet *tpp = NULL;
	uint16_t msglen;
	uint8_t dstmip;
	struct packetlist *pct = NULL;

	if((((src->sendinfo->nextSendSeqno)-(src->sendinfo->lastAckSeqno)) < WINDOW_SIZE) && (src->sendinfo->nextSendSeqno < src->sendinfo->nextAddSeqno)) {
		// Window size is not reached
		if(debug) fprintf(stderr, "MIPTP: Sending waiting data messages\n");
		getPacket(src->sendinfo->nextSendSeqno++, &pct, src->sendinfo->sendQueue);
		if(pct != NULL) {
			tpp = pct->data;
			msglen = pct->datalen;
			dstmip = pct->dst_mip;
		}
	}

	if(debug) fprintf(stderr, "MIPTP: nSend: %d | nAdd: %d | lAck: %d\n", src->sendinfo->nextSendSeqno, src->sendinfo->nextAddSeqno, src->sendinfo->lastAckSeqno);

	struct mipd_packet *mdp = NULL;
	if(tpp != NULL) mipdCreatepacket(dstmip, msglen, (char *)tpp, &mdp);
	*ret = mdp;
}

void getAckPacket(struct mipd_packet **ret, struct applist *src) {
	if(debug) fprintf(stderr, "MIPTP: getAckPacket(%p (%p), %p)\n", *ret, ret, src);

	struct packetlist *pl = NULL;
	getNextPacket(&pl, src->recvinfo->ackQueue);

	if(debug) fprintf(stderr, "MIPTP: Creating mipd packet, size %d, to %d\n", pl->datalen, pl->dst_mip);

	fprintf(stderr, "MIPTP: Created with port %d\n", pl->data->port);

	mipdCreatepacket(pl->dst_mip, pl->datalen, (char *)pl->data, ret);
	removeNextPacket(src->recvinfo->ackQueue);
}

void updateSeqnos(struct applist *src) {
	if((time(NULL)-src->sendinfo->lastAckTime) > timeout) {
		// Timeout without new ack, go back to previous ack
		src->sendinfo->nextSendSeqno = src->sendinfo->lastAckSeqno;
		src->sendinfo->lastAckTime = time(NULL);
		if(src->lastTimeout == 0) src->lastTimeout = time(NULL);
	}
	
	removeToSeqno(src->sendinfo->lastAckSeqno, src->sendinfo->sendQueue);
}

int doneSending(struct applist *src) {
	if(src->sendinfo->lastAckSeqno == src->sendinfo->nextSendSeqno && src->sendinfo->lastAckSeqno != 0) return 1;
	else if(src->sendinfo->nextSendSeqno == 0 && src->recvinfo->nextRecvSeqno != 0) return 1;
	else return 0;
}

int timedout(struct applist *src) {
	time_t tot = time(NULL)-src->lastTimeout;

	if(tot >= totTimeout && src->lastTimeout != 0) return 1;
	else return 0;
}