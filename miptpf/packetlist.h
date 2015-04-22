struct packetlist {
	struct tp_packet *data;
	uint16_t datalen;
	struct packetlist *next;
};

int addPacket(struct tp_packet *, uint16_t, struct packetlist *);
int getPacket(uint32_t, struct packetlist **, struct packetlist *);
int removeToSeqno(uint32_t, struct packetlist *);