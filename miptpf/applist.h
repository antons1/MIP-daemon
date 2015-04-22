struct sendinfo {
	uint32_t nextSendSeqno;
	uint32_t nextAddSeqno;
	uint8_t nextWindowPos;
	time_t lastAckTime;
	uint32_t lastAckSeqno;
	struct packetlist readyQueue[WINDOW_SIZE];
	struct packetlist *sendQueue;
};

struct recvinfo {
	uint32_t nextRecvSeqno;
	struct packetlist *recvQueue;
};

struct applist {
	uint16_t port;
	uint8_t fdind;
	struct sendinfo *sendinfo;
	struct recvinfo *recvinfo;
	struct applist *next;
};

int getNextApp(struct applist **);
int getApp(uint16_t, struct applist **);
int addApp(uint16_t, uint8_t, struct applist **);
int rmApp(uint16_t);