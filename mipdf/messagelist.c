#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "mipd.h"
#include "mip.h"

struct messagelist{
	char *msg;
	size_t msgsize;
	struct messagelist *next;
};

struct mllist {
	uint8_t id;					// Which id this list is for
	struct messagelist *mlist;	// Messagelist for assigned mip
	struct mllist *next;
};

/**
 * Adds a message to given messagelist
 * @param  amsg Message to be stored
 * @param  root Messagelist to store it in
 * @return      1 on success, 0 on failure
 */
int addmessage(char *amsg, size_t msglen, struct messagelist *root) {
	if(debug) fprintf(stderr, "MIPD: addmessage(%p, %zu, %p)\n", amsg, msglen, root);
	if(debug) fprintf(stderr, "MIPD: Adding message to messagelist\n");
	if(root == NULL) return 0;
	while(root->next != NULL) root = root->next;

	struct messagelist *next = malloc(sizeof(struct messagelist));
	next->msg = malloc(msglen);
	memcpy(next->msg, amsg, msglen);
	next->msgsize = msglen;
	next->next = NULL;

	root->next = next;

	return 1;
}

/**
 * Gets a message from given messagelist
 * @param  amsg Where the message is stored
 * @param  root Messagelist to get message from
 * @return      1 on success, 0 on failure
 */
int getmessage(char **amsg, size_t *msglen, struct messagelist *root) {
	if(debug) fprintf(stderr, "MIPD: getmessage()\n");
	if(root == NULL) return 0;
	if(root->next == NULL) return 0;


	struct messagelist *tmp = root->next;
	if(debug) fprintf(stderr, "MIPD: SIZE %zd - Retrieving message from messagelist\n", tmp->msgsize);

	*amsg = malloc(tmp->msgsize);
	memcpy(*amsg, tmp->msg, tmp->msgsize);
	*msglen = tmp->msgsize;

	root->next = tmp->next;
	free(tmp->msg);
	free(tmp);

	return 1;
}

/**
 * Checks whether any messages are stored in given messagelist
 * @param  root Messagelist to check
 * @return      1 if yes, 0 if no
 */
int hasmessage(const struct messagelist *root) {
	if(root == NULL) return 0;
	if(root->next != NULL) return 1;
	else return 0;
}

/**
 * Clears given list and frees resources
 * @param mlist List to clear
 */
void clearlist(struct messagelist *mlist) {
	if(mlist == NULL) return;
	if(mlist->next != NULL) clearlist(mlist->next);
	free(mlist->msg);
	free(mlist);
}

/**
 * Gets a messagelist from a list of messagelists
 * @param src      ID for the messagelist
 * @param mlsearch messagelistlist to search in
 * @param mlret    Where the found messagelist is stored
 */
void getmlist(uint8_t src, struct mllist *mlsearch, struct messagelist **mlret) {
	if(mlsearch == NULL) {
		return;
	}

	struct mllist *current = mlsearch->next;
	while(current != NULL) {
		if(current->id == src) break;

		current = current->next;
	}

	if(current == NULL) {
		current = mlsearch;
		while(current->next != NULL) current = current->next;
		current->next = malloc(sizeof(struct mllist));
		current = current->next;
		memset(current, 0, sizeof(struct mllist));
		current->next = NULL;
		current->id = src;
		current->mlist = malloc(sizeof(struct messagelist));
		memset(current->mlist, 0, sizeof(struct messagelist));
		current->mlist->next = NULL;
	}

	*mlret = current->mlist;
}

/**
 * Clears a list of messagelists, freeing resources
 * @param root mllist to clear
 */
void clearmlist(struct mllist *root) {
	struct mllist *next = root;

	if(next == NULL) return;

	if(next->next != NULL) clearmlist(next->next);
	clearlist(next->mlist);
	free(next);
}