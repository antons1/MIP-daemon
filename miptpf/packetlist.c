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
	struct packetlist *next;
};

int addPacket(struct tp_packet *, uint16_t, struct packetlist *);
int getPacket(uint32_t, struct packetlist **, struct packetlist *);
int removeToSeqno(uint32_t, struct packetlist *);

int addPacket(struct tp_packet *data, uint16_t datalen, struct packetlist *pl) {
	if(pl == NULL) return 0;
	if(data == NULL) return 0;

	while(pl->next != NULL) pl = pl->next;
	pl->next = malloc(sizeof(struct packetlist)),
	pl = pl->next;
	memset(pl, 0, sizeof(struct packetlist));
	pl->data = data;
	pl->datalen = datalen;
	pl->next = NULL;

	return 1;
}

int getPacket(uint32_t seqno, struct packetlist **result, struct packetlist *pl) {
	if(result == NULL) return 0;
	if(pl == NULL) return 0;
	if(pl->next == NULL) return 0;	// List is empty

	while(pl->next != NULL) {
		if(pl->next->data->seqno == seqno) {
			*result = pl->next;
			return 1;
		}

		pl = pl->next;
	}

	return 0;
}

int removeToSeqno(uint32_t seqno, struct packetlist *pl) {
	if(pl == NULL) return 0;

	int removed = 0;
	while(pl->next != NULL) {
		if(pl->next->data->seqno < seqno) {
			removed++;
			struct packetlist *tmp = pl->next;
			pl->next = tmp->next;
			free(tmp->data->content);
			free(tmp->data);
			free(tmp);
		} else break;

		pl = pl->next;
	}

	return removed;
}