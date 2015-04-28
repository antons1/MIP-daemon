#define UNIX_MAX_QUEUE 3
#ifndef UNIX_PATH_MAX
	#define UNIX_PATH_MAX 108
#endif

void readus(uint8_t, char *);
void sendus(size_t, uint8_t, uint8_t, char *);
int ushasmessage(uint8_t);
int usgetmessage(uint8_t, size_t *, char **);
void clearus();
void reard(char *, size_t);
void sendrd(uint8_t, uint8_t, size_t, char *);
void rinserdlist();