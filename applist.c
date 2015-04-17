#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "miptp.h"

struct applist {
	uint16_t port;
	uint32_t seqno;
	struct applist *next;
};

int getNextApp(struct applist **);
int getApp(uint16_t, struct applist **);
int addApp(uint16_t, struct applist **);
int rmApp(uint16_t);

int getNextApp(struct applist **ret) {

	return 0;
}

int getApp(uint16_t port, struct applist **ret) {

	return 0;
}

int addApp(uint16_t port, struct applist **ret) {
	
	return 0;
}

int rmApp(uint16_t port) {

	return 0;
}