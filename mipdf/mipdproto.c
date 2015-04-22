#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#define MIP_MAX_CONTENT_LEN 1496

struct mipd_packet {
	uint8_t dst_mip:8;
	uint16_t content_len:16;
	char content[0];
} __attribute__ ((packed));

int mipdCreatepacket(uint8_t, uint16_t, char *, struct mipd_packet **);
int mipdReadpacket(uint8_t *, uint16_t *, char **, struct mipd_packet *);

int mipdCreatepacket(uint8_t dstm, uint16_t cl, char *msg, struct mipd_packet **ret) {
	//fprintf(stderr, "MIPDPROTO: mipdCreatepacket(%d, %d, %p, %p (%p))\n", dstm, cl, msg, ret, *ret);
	size_t msgsz = sizeof(struct mipd_packet)+(cl);
	*ret = malloc(msgsz);
	memset(*ret, 0, msgsz);

	struct mipd_packet *tmp = *ret;

	tmp->dst_mip = dstm;
	tmp->content_len = cl;
	memcpy(tmp->content, msg, msgsz);

	return 1;
}

int mipdReadpacket(uint8_t *dstm, uint16_t *cl, char **msg, struct mipd_packet *ret) {
	//fprintf(stderr, "MIPDPROTO: mipdReadpacket(%p, %p, %p (%p), %p\n", dstm, cl, msg, *msg, ret);
	dstm = malloc(sizeof(uint8_t));
	cl = malloc(sizeof(uint16_t));

	*dstm = ret->dst_mip;
	*cl = ret->content_len;

	*msg = malloc((*cl));
	memcpy(*msg, ret->content, (*cl));

	return 1;
}