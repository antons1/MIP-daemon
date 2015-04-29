struct sendinfo {
	uint32_t nextSendSeqno;
	uint32_t nextAddSeqno;
	time_t lastAckTime;
	uint32_t lastAckSeqno;
	struct packetlist *sendQueue;
};

struct recvinfo {
	uint32_t nextRecvSeqno;
	struct packetlist *recvQueue;
	struct packetlist *ackQueue;
};

struct applist {
	uint16_t port;
	uint8_t fdind;
	time_t lastTimeout;
	uint8_t disconnected;
	struct sendinfo *sendinfo;
	struct recvinfo *recvinfo;
	struct applist *next;
};

int getNextApp(struct applist **);
int getApp(uint16_t, struct applist **);
int addApp(uint16_t, uint8_t, struct applist **);
int rmApp(uint16_t);
void freeAppList(struct applist *);