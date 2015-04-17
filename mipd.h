#define MAX_MIPS 10
#define PINGSID 1
#define PINGCID 2
#define RDID 3

extern char debug;
extern uint8_t mipaddrs[MAX_MIPS];
extern int nomips;

#ifndef MIP_MAX_LEN
#define MIP_MAX_LEN 1500
#endif


void printhwaddr(const char*); 
int islocalmip(uint8_t);