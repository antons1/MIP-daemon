#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "miptp.h"
#include "tpproto.h"

struct packetlist {
	struct tp_packet *data;
	uint16_t datalen;
	uint8_t dst_mip;
	struct packetlist *next;
};

int addPacket(struct tp_packet *, uint8_t, uint16_t, struct packetlist *);
int getPacket(uint32_t, struct packetlist **, struct packetlist *);
int getNextPacket(struct packetlist **, struct packetlist *);
int removeNextPacket(struct packetlist *);
int removeToSeqno(uint32_t, struct packetlist *);

int addPacket(struct tp_packet *data, uint8_t dstmip, uint16_t datalen, struct packetlist *pl) {
	if(debug) fprintf(stderr, "MIPTP: addPacket(%p, %d, %d, %p)\n", data, dstmip, datalen, pl);
	if(pl == NULL) return 0;
	if(data == NULL) return 0;

	while(pl->next != NULL) pl = pl->next;
	pl->next = malloc(sizeof(struct packetlist)),
	pl = pl->next;
	memset(pl, 0, sizeof(struct packetlist));
	pl->data = data;
	pl->datalen = datalen;
	pl->dst_mip = dstmip;
	pl->next = NULL;

	return 1;
}

int getPacket(uint32_t seqno, struct packetlist **result, struct packetlist *pl) {
	//if(debug) fprintf(stderr, "MIPTP: getPacket(%d, %p (%p), %p)\n", seqno, *result, result, pl);
	if(result == NULL) return 0;
	if(pl == NULL) return 0;
	if(pl->next == NULL) return 0;	// List is empty

	while(pl->next != NULL) {
		if(pl->next->data->seqno == seqno) {
			struct packetlist *tmp = pl->next;
			*result = malloc(sizeof(struct packetlist));
			memcpy(*result, tmp, sizeof(struct packetlist));

			struct packetlist *tmp2 = *result;
			tmp2->data = malloc(tmp->datalen);
			memcpy(tmp2->data, tmp->data, tmp->datalen);
			return 1;
		}

		pl = pl->next;
	}

	return 0;
}

int getNextPacket(struct packetlist **result, struct packetlist *pl) {
	if(debug) fprintf(stderr, "MIPTP: getNextPacket(%p (%p), %p)\n", result, *result, pl);
	*result = malloc(sizeof(struct packetlist)+pl->next->datalen);
	memcpy(*result, pl->next, sizeof(struct packetlist)+pl->next->datalen);
	if(*result == NULL) return 0;
	else return 1;
}

int removeNextPacket(struct packetlist *pl) {
	struct packetlist *toremove = pl->next;

	if(toremove == NULL) return 0;
	else {
		pl->next = toremove->next;
		free(toremove->data);
		free(toremove);
	}

	return 1;
}

int removeToSeqno(uint32_t seqno, struct packetlist *pl) {
	//if(debug) fprintf(stderr, "MIPTP: removeToSeqno(%d, %p)\n", seqno, pl);
	if(pl == NULL) return 0;

	int removed = 0;
	while(pl->next != NULL) {
		if(pl->next->data->seqno < seqno) {
			removed++;
			if(debug) fprintf(stderr, "MIPTP: Removed seqno %d\n", pl->next->data->seqno);
			struct packetlist *tmp = pl->next;
			pl->next = tmp->next;
			free(tmp->data);
			free(tmp);
		} else break;

		pl = pl->next;
		if(pl == NULL) break;
	}

	return removed;
}