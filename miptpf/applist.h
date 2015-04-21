struct applist {
	uint16_t port;
	uint32_t seqno;
	uint8_t fdind;
	struct applist *next;
};

int getNextApp(struct applist **);
int getApp(uint16_t, struct applist **);
int addApp(uint16_t, uint8_t, struct applist **);
int rmApp(uint16_t);