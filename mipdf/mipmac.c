// Includes
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>

#include "mipd.h"

// Structs
// MIP/MAC table
struct mmtable {
	char hwaddr[6];
	uint8_t mipaddr;
	struct mmtable *next;
};

static struct mmtable *root;

int findhw(char *, uint8_t, struct mmtable **);
int findmip(const char *, uint8_t *, struct mmtable **);
int rmmapping(struct mmtable *);
void printmap();

/**
 * Adds a mapping between a MIP and a MAC address. If a mapping between the two
 * already exists, nothing is done. If either the MIP or the MAC is already in
 * the table with a different mapping, it is removed before inserting the
 * new mapping. 
 * @param  hwaddr  MAC address to add
 * @param  mipaddr MIP address to add
 * @return         1 if mapping was added, 0 if not
 */
int addmapping(const char *hwaddr, const uint8_t mipaddr) {
	if(debug) fprintf(stderr, "MIPD: addmapping(%p, %d)\n", hwaddr, mipaddr);
	if(root == NULL) {
		root = malloc(sizeof(struct mmtable));
		memset(root, 0, sizeof(struct mmtable));
		root->next = NULL;
	}

	if(debug) {
		fprintf(stderr, "MIPD: Add MIP/MAC mapping between ");
		printhwaddr(hwaddr);
		fprintf(stderr, " and %d\n", mipaddr);
	}

	// Find existing mappings for the hwaddr and the mipaddr
	struct mmtable *exhw, *exmip;
	exhw = NULL;
	exmip = NULL;
	char tmp[6];
	uint8_t tmpm;
	int fexhw = findhw(tmp, mipaddr, &exhw);
	int fexmip = findmip(hwaddr, &tmpm, &exmip);

	// If they are not the same, remove them
	if(exhw != exmip) {
		if(fexhw == 1) rmmapping(exhw);
		if(fexmip == 1) rmmapping(exmip);
	} else if(fexhw == 1 && fexmip == 1) {
		if(debug) fprintf(stderr, "MIPD: Mapping already existed\n");
		return 0;
	}

	// Create the new mapping
	struct mmtable *current = root;
	while(current->next != NULL) current = current->next;

	current->next = malloc(sizeof(struct mmtable));
	memset(current->next, 0, sizeof(struct mmtable));
	current->next->next = NULL;

	current = current->next;

	memcpy(current->hwaddr, hwaddr, sizeof(current->hwaddr));
	current->mipaddr = mipaddr;

	if(debug) printmap();

	return 1;
}

/**
 * Finds an existing mapping between a MAC address and a MIP address.
 * If op is not NULL, the address of the table is put there. The MAC
 * address corresponding to the MIP address is put in hwaddr.
 * @param  hwaddr  Where the found hardware address is put
 * @param  mipaddr MIP address to search for
 * @param  op      Where the address to the table mapping is stored
 * @return         Returns 1 if the MAC was found, 0 if not
 */
int findhw(char *hwaddr, uint8_t mipaddr, struct mmtable **op) {
	//if(debug) fprintf(stderr, "MIPD: findhw(%p, %d, %p (%p))\n", hwaddr, mipaddr, op, *op);
	struct mmtable *current = root;
	if(current == NULL) return 0;

	if(debug) fprintf(stderr, "MIPD: Looking for MAC to MIP %d. ", mipaddr);

	while(current != NULL) {
		if(current->mipaddr == mipaddr) {
			if(debug) {
				fprintf(stderr, "Found ");
				printhwaddr(current->hwaddr);
				fprintf(stderr, "\n");
			}

			memcpy(hwaddr, current->hwaddr, sizeof(current->hwaddr));
			if(op != NULL) *op = current;
			return 1;
		}
		current = current->next;
	}

	if(debug) fprintf(stderr, "Not found\n");

	return 0;
}

/**
 * Finds an existing mapping between a MIP address and a MAC address.
 * If op is not NULL, the address of the table is put there. The MIP
 * address corresponding to the MAC address is put in mipaddr.
 * @param  hwaddr  MAC address to search for
 * @param  mipaddr MIP address that is found
 * @param  op      Where address to the table mapping is stored
 * @return         1 if MIP address is found, 0 if not
 */
int findmip(const char *hwaddr, uint8_t *mipaddr, struct mmtable **op) {
	if(debug) fprintf(stderr, "MIPD: findmip(%p, %p, %p (%p))\n", hwaddr, mipaddr, op, *op);
	struct mmtable *current = root;
	if(current == NULL) return 0;

	if(debug) {
		fprintf(stderr, "MIPD: Looking for MIP to MAC ");
		printhwaddr(hwaddr);
	}

	while(current != NULL) {
		if(memcmp(current->hwaddr, hwaddr, 6) == 0) {
			if(debug) fprintf(stderr, ". Found %d\n", current->mipaddr);
			memcpy(mipaddr, &(current->mipaddr), sizeof(unsigned char));
			if(op != NULL) *op = current;
			return 1;
		}

		current = current->next;
	}

	if(debug) fprintf(stderr, ". Not found\n");

	return 0;
}

/**
 * Removes the mapping given in rm.
 * @param  rm Mapping to be removed
 * @return    1 if it was removed, 0 if not
 */
int rmmapping(struct mmtable *rm) {
	if(debug) fprintf(stderr, "MIPD: rmmapping(%p)\n", rm);
	struct mmtable *current = root;
	if(current == NULL) return 0;
	if(rm == NULL) return 0;

	if(debug) {
		fprintf(stderr, "MIPD: Removing mapping between MIP %d and MAC ", current->mipaddr);
		printhwaddr(current->hwaddr);
		fprintf(stderr, ". ");
	}

	while(current != NULL) {
		if(current->next == rm) {
			struct mmtable *next = current->next;
			current->next = next->next;
			free(next);

			if(debug) fprintf(stderr, "Done\n");
			return 1;
		}

		current = current->next;
	}

	if(debug) fprintf(stderr, "Not done\n");

	return 0;
}

/**
 * Frees resources in the mip/mac table
 * @param del Used in recursion, should always be called with NULL
 */
void freemap(struct mmtable *del) {
	if(del == NULL) {
		del = root;
	}

	if(del->next != NULL) freemap(del->next);
	free(del);
}

/**
 * Prints the contents of the MIP/MAC map
 */
void printmap() {
	fprintf(stderr, "=========== MIP/MAC MAP ===========\n");
	if(root == NULL) {
		fprintf(stderr, "Map is empty and uninitialized\n");
		return;
	} else if(root->next == NULL) {
		fprintf(stderr, "Map is empty\n");
		return;
	}

	fprintf(stderr, "|   %s   |   %3s   |\n", "MAC              ", "MIP");
	fprintf(stderr, "===================================\n");
	struct mmtable *current = root;
	while((current = current->next) != NULL) {
		fprintf(stderr, "|   ");
		printhwaddr(current->hwaddr);
		fprintf(stderr, "   |   %3d   |\n", current->mipaddr);
	}

	fprintf(stderr, "===================================\n");

}