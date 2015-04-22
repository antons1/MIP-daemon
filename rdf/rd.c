#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>

#include "../mipdf/mipdproto.h"

#define FID 1
#define BC_INTERVAL 30
#define INVALID_TIMER 180
#define FLUSH_TIMER 240
#define UPD_INTERVAL 5
#define BC_RAND 3

struct routing_table {
	uint8_t change_flag;
	uint8_t local_mip;
	uint8_t dst_mip;
	uint8_t next_mip;
	uint8_t cost;
	time_t timer;
	struct routing_table *next;
};

struct route_inf {
	uint8_t cost:5;
	uint8_t padding:3;
	uint8_t mip:8;
} __attribute__ ((packed));

struct route_dg {
	uint8_t src_mip:8;
	uint8_t local_mip:8;
	uint8_t mode:1;
	uint8_t records_len:7;
	struct route_inf records[0];
} __attribute__ ((packed));

// Packetlist struct and functions
struct packetlist {
	struct route_dg *packet;
	time_t sent;
	struct packetlist *next;
};

int hasmessage();
void getmessage(struct route_dg **);
void addmessage(struct route_dg *);

int procargs(int, char*[]);										// Processes the arguments
void recvdata(char *);											// Get data from socket
void respmip(struct route_dg *);								// Respond to MIP daemon request
void resproute(struct route_dg *);								// Respond to Routing Daemon request
void procresp(struct route_dg *);								// Process incoming response
void addroute(uint8_t, uint8_t, uint8_t, uint8_t);				// Adds a record to the table
void gettable(struct route_dg *);								// Gets the full routing table
void getroute(struct route_dg *, uint8_t);						// Gets the record for given MIP 
void sendbroadcast();											// Broadcasts the table
void sendupdate();												// Sends an update
void rinsetable();												// Checks timers in table
void printtable();

struct routing_table *rtable;
int maxsize;
struct packetlist *plist;

uint8_t lmip;					// Local MIP address
time_t lastbroadcast;			// Last time broadcast was sent
time_t lastupdate;				// Last time update was sent
uint8_t changeflag;				// Whether changes has occured since last update/broadcast

int debug = 0;					// Whether debug messages are printed
int prtb = 1;					// Whether the table is printed on every update

/**
 * Main
 * @param  argc Argument count
 * @param  argv Arguments
 * @return      0 on success, something else on fail
 */
int main(int argc, char *argv[]) {
	int res = procargs(argc, argv);
	if(res < 0) return 1;

	lastbroadcast = 0;
	lastupdate = 0;
	lmip = res;

	maxsize = ((1490-sizeof(struct route_dg))/sizeof(struct route_inf));

	// Connect socket to MIP daemon
	struct sockaddr_un mipdaddr;
	mipdaddr.sun_family = AF_UNIX;
	sprintf(mipdaddr.sun_path, "socket%d", res);

	int mipconn = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if(mipconn == -1) {
		perror("ROUTE: Error creating socket");
		return 1;
	}

	if(connect(mipconn, (struct sockaddr *)&mipdaddr, sizeof(struct sockaddr_un)) == -1) {
		close(mipconn);
		perror("ROUTE: Error connecting to socket");
		return 1;
	}

	struct mipd_packet *id;
	mipdCreatepacket(FID, 0, "", &id);

	if(send(mipconn, (char *)id, sizeof(struct mipd_packet), 0) == -1) {
		perror("ROUTE: Error identifying to MIP daemon");
		close(mipconn);
		return 1;
	}

	rtable = malloc(sizeof(struct routing_table));
	memset(rtable, 0, sizeof(struct routing_table));
	rtable->next = NULL;

	struct pollfd fds[1];

	fds[0].fd = mipconn;
	fds[0].events = POLLIN | POLLOUT | POLLHUP;

	struct route_dg *hello = malloc(sizeof(struct route_dg));
	hello->mode = 0;
	hello->src_mip = 0;
	hello->local_mip = 0;
	hello->records_len = 1;
	memset(hello->records, 0, sizeof(hello->records));

	struct mipd_packet *hellowrap;
	mipdCreatepacket(lmip, sizeof(struct route_dg), (char *)hello, &hellowrap);

	srand(time(NULL));

	if(send(fds[0].fd, (char *)hellowrap, sizeof(struct route_dg), 0) == -1) {
		perror("ROUTE: Error saying hello");
		close(mipconn);
		return 1;
	} else {
		if(debug) printf("ROUTE: Said hello.\n");
	}


	while(1) {
		int rb = poll(fds, 1, 0);

		if(rb != 0) {
			if(fds[0].revents & POLLHUP) {
				if(debug) printf("ROUTE: MIP daemon closed connection\n");
				break;
			}

			if(fds[0].revents & POLLIN) {
				if(debug) printf("ROUTE: Recieving data on socket\n");
				char buf[1500];
				read(fds[0].fd, buf, 1500);
				struct mipd_packet *mp = (struct mipd_packet *)buf;

				recvdata(mp->content);
			}
		}

		if(hasmessage()) {
			if(debug) printf("ROUTE: Sending data on socket\n");
			struct route_dg *buf;
			getmessage(&buf);

			if(debug) printf("ROUTE: SRC %d | LCL %d | MOD %d | LEN %d\n", buf->src_mip, buf->local_mip, buf->mode, buf->records_len);
			
			struct mipd_packet *mp;
			struct route_dg *tmp = (struct route_dg *)buf;
			size_t msgsz = sizeof(struct route_dg)+(sizeof(struct route_inf)*tmp->records_len);
			
			mipdCreatepacket(lmip, msgsz, (char *)buf, &mp);
			send(fds[0].fd, mp, msgsz, 0);
		}

		if(changeflag && (time(NULL)-UPD_INTERVAL) >= lastupdate) {
			// Changes has occured, and update timer is expired
			if(debug) printf("ROUTE: Broadcasting updated routes, timer was %d\n", (int)(time(NULL)-lastupdate));
			sendupdate();
			lastupdate = time(NULL);
			changeflag = 0;

			if(debug) printf("ROUTE: Update timer reset\n");
		}

		if((time(NULL)-BC_INTERVAL) >= lastbroadcast) {
			// Broadcast timer has expired
			if(debug) printf("ROUTE: Broadcasting table, timer was %d\n", (int)(time(NULL)-lastbroadcast));
			sendbroadcast();
			int rm = rand()%BC_RAND+1;
			int pn = rand()%2;
			lastbroadcast = time(NULL);

			if(pn) lastbroadcast -= rm;
			else lastbroadcast += rm;

			if(debug) printf("ROUTE: Broadcast timer reset, now waiting %d seconds\n", (int)((lastbroadcast+BC_INTERVAL)-time(NULL)));
			if(debug || prtb) printtable();
		}

		rinsetable();
	}

	close(mipconn);

	return 0;
}

/**
 * Processes arguments given at program start, and displays error message if they are wrong
 * @param  argc Argument count
 * @param  argv Arguments
 * @return      -1 if something was wrong, local mip if not
 */
int procargs(int argc, char *argv[]) {
	if(argc < 2) {
		printf("%s: error: Too few arguments\n", argv[0]);
		printf("%s: usage: %s [Local MIP]\n", argv[0], argv[0]);
		return -1;
	}

	int lmip = atoi(argv[1]);
	if((lmip == 0 && strcmp(argv[1], "0") != 0) || strncmp(argv[1], "-", 1) == 0) {
		printf("%s: error: Local MIP must be between 0 - 255. %s is not valid.\n", argv[0], argv[1]);
		printf("%s: usage: %s [Local MIP]\n", argv[0], argv[0]);
		return -1;
	}

	return lmip;
}

/**
 * Reads the inital data in a recieved datagram, passing the message on to correct handler
 * @param msg Recieved message
 */
void recvdata(char *msg) {
	if(debug) printf("ROUTE: recvdata(%p)\n", msg);
	struct route_dg *rd = (struct route_dg *)msg;

	if(debug) printf("ROUTE: Packet: SRC: %d | LCL: %d | LEN: %d | MOD: %d\n", rd->src_mip, rd->local_mip, rd->records_len, rd->mode);

	if(rd->mode == 0 && (rd->src_mip == rd->local_mip)) {
		if(debug) printf("ROUTE: Recieved request for route from MIP daemon\n");
		respmip(rd);
	} else if(rd->mode == 0) {
		if(debug) printf("ROUTE: Recieved request for route from external RD\n");
		resproute(rd);
	} else {
		if(debug) printf("ROUTE: Recieved route from external RD\n");
		procresp(rd);
	}
}

/**
 * Responds to a route request from the local MIP daemon
 * @param dg Route datagram to be sent
 */
void respmip(struct route_dg *dg){
	if(debug) printf("ROUTE: respmip(%p)\n", dg);
	
	getroute(dg, dg->records[0].mip);

	addmessage(dg);

}

/**
 * Responds to a route request from an external Routing Daemon
 * @param dg Route datagram to be sent
 */
void resproute(struct route_dg *dg) {
	if(debug) printf("ROUTE: resproute(%p)\n", dg);
	addroute(dg->src_mip, dg->local_mip, dg->local_mip, 0);

	gettable(dg);

	dg->mode = 1;

	addmessage(dg);
}

/**
 * Processes a route response from an external Routing Daemon
 * @param dg The datagram to be sent
 */
void procresp(struct route_dg *dg) {
	if(debug) printf("ROUTE: procresp(%p)\n", dg);
	int i = 0;
	for(; i < dg->records_len; i++) {
		addroute(dg->src_mip, dg->local_mip, dg->records[i].mip, dg->records[i].cost);
	}

}

/**
 * Adds a route to the routing table, implementing Dijkstras algorithm to select
 * the shortest path.
 * @param local Local interface route starts with
 * @param next  Next hop on route
 * @param fdest Final destination of route
 * @param cost  Cost of route
 */
void addroute(uint8_t local, uint8_t next, uint8_t fdest, uint8_t cost) {
	if(debug) printf("ROUTE: addroute(%d, %d, %d, %d)\n", local, next, fdest, cost);
	struct routing_table *rt = rtable;

	while(rt->next != NULL) {
		if(rt->next->dst_mip == fdest) break;
		
		rt = rt->next;
	}

	if(rt->next == NULL) {
		// We have no previous record for this one
		rt->next = malloc(sizeof(struct routing_table));
		rt = rt->next;
		memset(rt, 0, sizeof(struct routing_table));
		rt->next = NULL;
		rt->local_mip = local;
		rt->next_mip = next;
		rt->dst_mip = fdest;
		if(cost == 31) rt->cost = 0;
		else rt->cost = cost+1;
		rt->timer = time(NULL);
		rt->change_flag = 1;

		if(debug) printf("ROUTE: Route to %d was added\n", rt->dst_mip);

		changeflag = 1;
	
	} else {
		// We have a previous record
		if(local == rt->next->local_mip && next == rt->next->next_mip && fdest == rt->next->dst_mip && cost+1 == rt->next->cost) {
			if(debug) printf("ROUTE: To %d from %d via %d recieved and affirmed\n", fdest, local, next);
			rt->next->timer = time(NULL);
		} else if(local == rt->next->local_mip && next == rt->next->next_mip && fdest == rt->next->dst_mip && cost >= 16) {
			if(debug) printf("ROUTE: To %d from %d via %d recieved as inaccessible\n", fdest, local, next);
			rt->next->cost = cost;
			rt->next->change_flag = 1;
			changeflag = 1;
		}

		if(cost+1 < rt->next->cost) {
			// We have recieved a faster route, update the record
			rt = rt->next;
			rt->local_mip = local;
			rt->next_mip = next;
			rt->dst_mip = fdest;
			rt->cost = cost+1;
			rt->timer = time(NULL);
			rt->change_flag = 1;
			changeflag = 1;

			if(debug) printf("ROUTE: Route to %d was updated (new cost %d)\n", rt->dst_mip, rt->cost);
			
		} else if(debug) printf("ROUTE: Route to %d was discarded\n", fdest);
	}

	if(debug) printtable();
}

/**
 * Prints the routing table to stdout
 */
void printtable() {
	printf("=========== ROUTING TABLE ===========\n");
	printf("| LCL | NXT | DST | CST | TMR | CNG |\n");
	printf("=====================================\n");

	struct routing_table *rt = rtable->next;
	while(rt != NULL) {
		printf("| %3d | %3d | %3d | %3d | %3d | %3d |\n", rt->local_mip, rt->next_mip, rt->dst_mip, rt->cost, (int)time(NULL)-(int)rt->timer, rt->change_flag);
		rt = rt->next;
	}

	printf("=====================================\n");
}

/**
 * Fills fill->records with all records from the routing table, except records
 * where next_mip == fill->src_mip to implement split horizon
 * @param fill Datagram to be sent
 */
void gettable(struct route_dg *fill) {
	if(debug) printf("ROUTE: gettable(%p)\n", fill);
	struct routing_table *rt = rtable->next;

	fill->records_len = 0;

	while(rt != NULL) {
		if(rt->next_mip != fill->src_mip) { 
			fill->records[(fill->records_len)].cost = rt->cost;
			fill->records[(fill->records_len)++].mip = rt->dst_mip;
		}

		rt = rt->next;
	}

}

/**
 * Gets the route to dst from the routing table, and stores it in
 * fill->records[0]
 * @param fill The datagram to be sent
 * @param dst  Destination to find route to
 */
void getroute(struct route_dg *fill, uint8_t dst) {
	if(debug) printf("ROUTE: getroute(%p, %d)\n", fill, dst);
	struct routing_table *rt = rtable->next;

	while(rt != NULL) {
		if(rt->dst_mip == dst) {
			break;
		}

		rt = rt->next;
	}

	if(rt == NULL) {
		fill->records[0].cost = 16;
		fill->src_mip = lmip;
		fill->local_mip = 0;
	} else {
		fill->records[0].cost = rt->cost;
		fill->src_mip = rt->local_mip;
		fill->local_mip = rt->next_mip;
	}

}

/**
 * Sends update of all routes that has changed since last update
 */
void sendupdate() {
	if(debug) printf("ROUTE: sendupdate()\n");

	struct routing_table *outer = rtable->next;

	while(outer != NULL) {
		if(outer->cost == 1) {
			struct route_dg *rd = malloc(sizeof(struct route_dg)+(maxsize*sizeof(struct route_inf)));
			memset(rd, 0, sizeof(struct route_dg)+(maxsize*sizeof(struct route_inf)));
			rd->src_mip = outer->next_mip;
			rd->local_mip = outer->local_mip;
			rd->mode = 1;

			struct routing_table *rt = rtable->next;

			while(rt != NULL) {
				if(rt->change_flag == 1 && rt->next_mip != rd->src_mip) {
					rd->records[(rd->records_len)].mip = rt->dst_mip;
					rd->records[(rd->records_len)++].cost = rt->cost;
					rt->change_flag = 0;
				}

				rt = rt->next;
			}

			addmessage(rd);
		}

		outer = outer->next;
	}
}

/**
 * Sends broadcast of all routes on all interfaces
 */
void sendbroadcast() {
	if(debug) printf("ROUTE: sendbroadcast()\n");

	struct routing_table *rt = rtable->next;

	while(rt != NULL){
		if(rt->cost == 1) {
			struct route_dg *rd = malloc(sizeof(struct route_dg)+(maxsize*sizeof(struct route_inf)));
			memset(rd, 0, sizeof(struct route_dg)+(maxsize*sizeof(struct route_inf)));
			rd->src_mip = rt->next_mip;
			rd->local_mip = rt->local_mip;
			rd->mode = 1;
		
			gettable(rd);
			addmessage(rd);
		}

		rt = rt->next;
	}
}

/**
 * Checks the routing table, invalidating timed out routes, and removing invalid routes. 
 * Also resets timer for local MIPS
 */
void rinsetable() {
	struct routing_table *rt = rtable;

	while(rt->next != NULL) {
		if(rt->next->cost == 0) rt->next->timer = time(NULL);
		if((time(NULL)-(rt->next->timer)) >= INVALID_TIMER && rt->next->cost != 16) {
			rt->next->cost = 16;
			rt->change_flag = 1;
			changeflag = 1;
			if(debug) printf("ROUTE: Route to %d is now invalid\n", rt->next->dst_mip);
		} else if((time(NULL)-(rt->next->timer)) >= FLUSH_TIMER) {
			struct routing_table *tmp = rt->next;
			rt->next = tmp->next;
			if(debug) printf("ROUTE: Route to %d is now deleted\n", rt->next->dst_mip);
			free(tmp);
		}

		if(rt == NULL || rt->next == NULL) break;
		rt = rt->next;
	}
}

/**
 * Checks if any messages are queued
 * @return 1 if yes, 0 if no
 */
int hasmessage() {
	if(plist == NULL) {
		plist = malloc(sizeof(struct packetlist));
		memset(plist, 0 , sizeof(struct packetlist));
		plist->next = NULL;
	}

	return plist->next == NULL ? 0 : 1;
}

/**
 * Gets a message from the queue, and stores it in rd
 * @param rd Where to store the message
 */
void getmessage(struct route_dg **rd) {
	if(plist == NULL) {
		plist = malloc(sizeof(struct packetlist));
		memset(plist, 0 , sizeof(struct packetlist));
		plist->next = NULL;
	}

	struct packetlist *pl = plist;
	while(pl->next->next != NULL) pl = pl->next;

	*rd = pl->next->packet;
	struct packetlist *tmp = pl->next;
	pl->next = pl->next->next;
	free(tmp); 
}

/**
 * Adds a message to the queue
 * @param rd Message to be stored
 */
void addmessage(struct route_dg *rd) {
	if(plist == NULL) {
		plist = malloc(sizeof(struct packetlist));
		memset(plist, 0 , sizeof(struct packetlist));
		plist->next = NULL;
	}

	struct packetlist *tmp = malloc(sizeof(struct packetlist));
	memset(tmp, 0, sizeof(struct packetlist));
	tmp->packet = rd;
	tmp->sent = time(NULL);
	tmp->next = plist->next;
	plist->next = tmp;
}