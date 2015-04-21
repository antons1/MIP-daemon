#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "miptp.h"

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
void initroot();

static struct applist *root;
static struct applist *current;

int getNextApp(struct applist **ret) {
	initroot();
	if(current == NULL) {
		current = root;
	}

	current = current->next;
	*ret = current;

	return (*ret == NULL) ? 0 : 1;
}

int getApp(uint16_t port, struct applist **ret) {
	initroot();

	struct applist *srch = root;
	while(srch->next != NULL) {
		if(srch->next->port == port) {
			if(ret != NULL) *ret = srch->next;
			return 1;
		}

		srch = srch->next;
	}

	return 0;
}

int addApp(uint16_t port, uint8_t fdind, struct applist **ret) {
	initroot();

	struct applist *srch = root;
	while(srch->next != NULL) srch = srch->next;

	srch->next = malloc(sizeof(struct applist));
	srch = srch->next;
	memset(srch, 0, sizeof(struct applist));

	srch->port = port;
	srch->fdind = fdind;
	srch->next = NULL;
	if(ret != NULL)	*ret = srch;

	return 1;
}

int rmApp(uint16_t port) {
	initroot();

	struct applist *srch = root;
	while(srch->next != NULL) {
		if(srch->next->port == port) {
			struct applist *tmp = srch->next;
			srch->next = tmp->next;

			if(current->port == tmp->port) {
				current = srch;
			}

			free(tmp);
			return 1;
		}

		srch = srch->next;
		if(srch == NULL) break;
	}

	return 0;
}

void initroot() {
	if(root == NULL) {
		root = malloc(sizeof(struct applist));
		memset(root, 0, sizeof(struct applist));
		root->next = NULL;
	}
}