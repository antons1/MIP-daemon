struct messagelist{
	char *msg;
	struct messagelist *next;
};

struct mllist {
	uint8_t id;					// Which id this list is for
	struct messagelist *mlist;	// Messagelist for assigned mip
	struct mllist *next;
};

int addmessage(char *, struct messagelist *);
int getmessage(char **, struct messagelist *);
int hasmessage(const struct messagelist *);
void clearlist(struct messagelist *);
void getmlist(uint8_t, struct mllist *, struct messagelist **);
void clearmlist(struct mllist *);