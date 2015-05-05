// Definitions
#define MIP_MAX_LEN 1500
#define ETH_P_MIP 65535
#define TTL_MAX 15

// Structs
// Ethernet frame struct
struct eth_frame {
	char dst_addr[6];
	char src_addr[6];
	char eth_proto[2];
	char content[0];
} __attribute__ ((packed));

// MIP frame struct 
struct mip_frame {
	uint8_t tra_bits:3;
	uint8_t ttl:4;
	uint16_t content_len:9;
	uint8_t src_addr:8;
	uint8_t dst_addr:8;
	char content[0];
} __attribute__ ((packed));

int miphasmessage(uint8_t);
void readmip(char *, uint8_t);
void clearmip();
void rinsearplist();
void sendmip(uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, size_t, char *);
int mipgetmessage(uint8_t, size_t *, char **);
void sendroute(uint8_t, uint8_t, uint8_t, size_t, char *);
void sendtransport(uint8_t, uint8_t, int16_t, int16_t, size_t, char *);