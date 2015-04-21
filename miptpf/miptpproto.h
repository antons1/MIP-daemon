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