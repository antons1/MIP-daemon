struct route_inf {
	uint8_t cost:5;
	uint8_t padding:3;
	uint8_t mip:8;
} __attribute__ ((packed));

// Can be exchanged with the PING protocol.
struct route_dg {
	uint8_t src_mip:8;
	uint8_t local_mip:8;
	uint8_t mode:1;
	uint8_t records_len:7;
	struct route_inf records[0];
} __attribute__ ((packed));