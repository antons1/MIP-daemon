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
void freePacketList(struct packetlist *);
int containsSeqno(uint32_t, struct packetlist *);

/**
 * Adds a packet to a packet list.
 * @param  data    Packet to store
 * @param  dstmip  Destination for stored packet
 * @param  datalen Length of stored packet
 * @param  pl      Packet list to add packet to
 * @return         1 on success, 0 on error
 */
int addPacket(struct tp_packet *data, uint8_t dstmip, uint16_t datalen, struct packetlist *pl) {
	//if(debug) fprintf(stderr, "MIPTP: addPacket(%p, %d, %d, %p)\n", data, dstmip, datalen, pl);
	if(pl == NULL) return 0;
	if(data == NULL) return 0;

	struct packetlist *pl2 = pl;
	while(pl->next != NULL) {
		if(pl->next->data->seqno < data->seqno) pl = pl->next;
		else if(pl->next->data->seqno == data->seqno) return 0;
		else break;
	}

	if(debug) fprintf(stderr, "MIPTP: Adding packet to packetlist: ");

	if(pl2 != pl && debug) fprintf(stderr, "After SN %d, ", pl->data->seqno);
	else if(debug) fprintf(stderr, "As root, ");
		
	struct packetlist *tmp = pl->next;
	pl->next = malloc(sizeof(struct packetlist));
	pl = pl->next;
	memset(pl, 0, sizeof(struct packetlist));
	pl->data = data;
	pl->datalen = datalen;
	pl->dst_mip = dstmip;
	pl->next = tmp;

	if(debug)fprintf(stderr, "Added SN %d, ", pl->data->seqno);
	if(pl->next != NULL && debug) fprintf(stderr, "Befor SN %d\n", pl->next->data->seqno);
	else if(debug) fprintf(stderr, "At the end\n");
	return 1;
}

/**
 * Gets a packet from a packet list
 * @param  seqno  Sequence number to get
 * @param  result Where the packet is stored
 * @param  pl     Packet list to get packet from
 * @return        1 on success (packet found), 0 on error
 */
int getPacket(uint32_t seqno, struct packetlist **result, struct packetlist *pl) {
	//if(debug) fprintf(stderr, "MIPTP: getPacket(%d, %p (%p), %p)\n", seqno, *result, result, pl);
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

/**
 * Gets the next packet from a packet list
 * @param  result Where the packet is stored
 * @param  pl     Packet list where packet is
 * @return        1 on success, 0 on error
 */
int getNextPacket(struct packetlist **result, struct packetlist *pl) {
	//if(debug) fprintf(stderr, "MIPTP: getNextPacket(%p (%p), %p)\n", result, *result, pl);
	*result = pl->next;
	
	if(*result == NULL) return 0;
	else return 1;
}

/**
 * Removes the next packet from the packet list
 * @param  pl Packet list to remove data from
 * @return    1 on success, 0 on error
 */
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

/**
 * Removes packets up to, but not including, given sequence number. It is assumed that
 * packets are stored in consequtive order in the packet list
 * @param  seqno Seqence number limit
 * @param  pl    Packet list to delete packets from
 * @return       0 on error, or amount of deleted packets
 */
int removeToSeqno(uint32_t seqno, struct packetlist *pl) {
	//if(debug) fprintf(stderr, "MIPTP: removeToSeqno(%d, %p)\n", seqno, pl);
	if(pl == NULL) return 0;

	int removed = 0;
	while(pl->next != NULL) {
		if(pl->next->data->seqno < seqno) {
			removed++;
			//if(debug) fprintf(stderr, "MIPTP: Removed seqno %d\n", pl->next->data->seqno);
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

/**
 * Frees up all resources used for given packetlist
 * @param pl Packet list to free
 */
void freePacketList(struct packetlist *pl) {
	if(pl->next != NULL) freePacketList(pl->next);

	free(pl->data);
	free(pl);
}

/**
 * Checks whether a packet list contains a given sequence number
 * @param  seqno Sequence number to search for
 * @param  pl    Packet list to search in
 * @return       1 if packet exists, 0 if not
 */
int containsSeqno(uint32_t seqno, struct packetlist *pl) {
	while(pl->next != NULL) {
		if(pl->next->data->seqno == seqno) return 1;
		else if(pl->next->data->seqno > seqno) return 0;

		pl = pl->next;
	}

	return 0;
}