// Structs
struct mmtable {
	char hwaddr[6];
	uint8_t mipaddr;
	struct mmtable *next;
};

// Function prototypes
int addmapping(const char *, const uint8_t);
int findhw(char *, uint8_t, struct mmtable **);
int findmip(const char *, uint8_t *, struct mmtable **);
void freemap(struct mmtable *);
void printmap();