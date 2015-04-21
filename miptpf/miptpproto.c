#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "miptp.h"

#define MIPTP_MAX_CONTENT_LEN 1492

// Protocol used between app and MIP-TP
struct miptp_packet {
	uint8_t dst_mip:8;
	uint16_t dst_port:14;
	uint16_t content_len:10;
	char content[0];
} __attribute__ ((packed));

int miptpCreatepacket(uint8_t, uint16_t, uint16_t, char *, struct miptp_packet **);
int miptpReadpacket(uint8_t *, uint16_t *, uint16_t *, char **, struct miptp_packet *);

int miptpCreatepacket(uint8_t dstm, uint16_t dstp, uint16_t cl, char *msg, struct miptp_packet **create) {
	if(cl > MIPTP_MAX_CONTENT_LEN) return 0;
	else {
		size_t msgsz = sizeof(struct miptp_packet)+cl;
		*create = malloc(msgsz);
		memset(*create, 0, msgsz);

		struct miptp_packet *tmp = *create;

		tmp->dst_mip = dstm;
		tmp->dst_port = dstp;
		tmp->content_len = cl;
		memcpy(tmp->content, msg, cl);

		return 1;
	}
}

int miptpReadpacket(uint8_t *dstm, uint16_t *dstp, uint16_t *cl, char **msg, struct miptp_packet *readp) {
	if(readp == NULL) return 0;
	else {
		dstm = malloc(sizeof(uint8_t));
		dstp = malloc(sizeof(uint16_t));
		cl = malloc(sizeof(uint16_t));
		
		memset(dstm, 0, sizeof(uint8_t));
		memset(dstp, 0, sizeof(uint16_t));
		memset(cl, 0, sizeof(uint8_t));

		*dstm = readp->dst_mip;
		*dstp = readp->dst_port;
		*cl = readp->content_len;

		*msg = malloc(*cl);
		memset(*msg, 0, (size_t)*cl);
		memcpy(*msg, readp->content, (size_t)*cl);

		return 0;
	}
}