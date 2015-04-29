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