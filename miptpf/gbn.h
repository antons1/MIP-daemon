void recvApp(struct miptp_packet *, struct applist *);
void recvMip(struct mipd_packet *);
void recvAck(struct tp_packet *);
void recvData(struct tp_packet *, uint16_t);
int hasSendData(struct applist *);
int hasRecvData(struct applist *);
void sendAck(struct applist *, uint16_t);
void getAppPacket(struct miptp_packet **, struct applist *);
void getMipPacket(struct mipd_packet **, struct applist *);