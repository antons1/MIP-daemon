struct tp_packet {
	uint8_t pl_bits:2;
	uint16_t port:14;
	uint16_t seqno:16;
	char content[0];
} __attribute__ ((packed));