#define MIP_MAX_CONTENT_LEN 1496

struct mipd_packet {
	uint8_t dst_mip:8;
	uint16_t content_len:16;
	char content[0];
} __attribute__ ((packed));

int mipdCreatepacket(uint8_t, uint16_t, char *, struct mipd_packet **);
int mipdReadpacket(uint8_t *, uint16_t *, char **, struct mipd_packet *);