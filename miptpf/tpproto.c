#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "miptp.h"

struct tp_packet {
	uint8_t pl_bits:2;
	uint16_t port:14;
	uint16_t seqno:16;
	char content[0];
} __attribute__ ((packed));

void tpCreatepacket(uint8_t, uint16_t, uint16_t, uint16_t, char *, struct tp_packet **);

void tpCreatepacket(uint8_t pl, uint16_t port, uint16_t seqno, uint16_t msglen, char *msg, struct tp_packet **create) {
	size_t msgsz = sizeof(struct tp_packet)+msglen+pl;
	*create = malloc(msgsz);
	memset(*create, 0, msgsz);

	memcpy((*create)->content, msg, msglen);
	(*create)->pl_bits = pl;
	(*create)->port = port;
	(*create)->seqno = seqno;
}