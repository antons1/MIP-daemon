#define UNIX_MAX_QUEUE 3
#ifndef UNIX_PATH_MAX
	#define UNIX_PATH_MAX 108
#endif

void readus(uint8_t, char *);
void sendus(char *);
int ushasmessage(uint8_t);
int usgetmessage(uint8_t, char **);
void clearus();
void reard(char *);
void sendrd(uint8_t, uint8_t, char *);
void rinserdlist();

struct us_frame {
	uint8_t src_addr:8;
	uint8_t dst_addr:8;
	uint8_t mode:1;
	uint8_t padding:7;
	char content[0];
} __attribute__ ((packed));