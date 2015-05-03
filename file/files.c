#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/un.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "../miptpf/miptpproto.h"
#define F_MAX_LEN 1492

int checkargs(int, char *[]);
int checknum(int, char *, int, int);

int lmip;
int lport;
char *filename;

/**
 * Main function for file server
 * @param  argc Number of arguments
 * @param  argv The arguments
 * @return      0 on success
 */
int main(int argc, char *argv[]) {
	int args = checkargs(argc, argv);
	if(!args) return 1;

	int sock = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(sock == -1) {
		perror("socket()");
		return 1;
	}

	struct sockaddr_un laddr;
	laddr.sun_family = AF_UNIX;
	sprintf(laddr.sun_path, "socket%d_tp", lmip);

	int err = connect(sock, (struct sockaddr *)&laddr, sizeof(struct sockaddr_un));

	if(err == -1) {
		perror("connect()");
		return 1;
	}

	struct miptp_packet *identify;
	uint8_t dstm = lmip;
	uint16_t dstp = lport;
	uint16_t cl = 0;
	char *msg = malloc(0);

	miptpCreatepacket(dstm, dstp, cl, msg, &identify);
	err = write(sock, (char *)identify, sizeof(struct miptp_packet)+cl);

	if(err == -1){ 
		perror("write()");
		return 1;
	}

	int filefd = 0;
	char *curpath = malloc(256);
	memset(curpath, 0, 256);
	if(getcwd(curpath, 256) == NULL) {
		perror("FILES: Getting working directory");
		return 1;
	}

	char *filepath = malloc(strlen(filename)+strlen(curpath)+2);
	sprintf(filepath, "%s/%s", curpath, filename);

	char buf[1500];
	int i = 0;
	while(1){
		ssize_t rb = recv(sock, buf, 1500, 0);
		if(rb <= 0) {
			perror("FILES: Reading from MIP");
			break;
		}

		struct miptp_packet *tpp = (struct miptp_packet *)buf;
		if(filefd == 0) {
			filefd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC);
			if(filefd == -1) {
				perror("FILES: Opening file for writing");
				break;
			}
		}

		ssize_t wb = write(filefd, tpp->content, tpp->content_len);
		if(wb == -1) {
			perror("FILES: Writing to file");
			break;
		} else {
			printf("FILES: Seq %d - Recieved %zd bytes.\n", i++, wb);
		}

		if(tpp->content_len < F_MAX_LEN) {
			printf("FILES: End of file reached\n");
			break;
		}

	}

	free(curpath);
	free(filepath);
	if(filefd != 0) close(filefd);
	close(sock);

	return 0;
}

/**
 * Checks arguments given to program, that there are enough, and that they are valid, or prints error message
 * @param  argc Number of arguments
 * @param  argv Arguments given
 * @return      0 on error, 1 on success
 */
int checkargs(int argc, char *argv[]) {
	char *errmsg = malloc(1024);
	char *usemsg = malloc(1024);
	int error = 0;

	memset(errmsg, 0, 1024);
	memset(usemsg, 0, 1024);

	sprintf(usemsg, "%s [Local MIP] [Port] [filename]", argv[0]);

	if(argc < 4) {
		sprintf(errmsg, "Too few arguments");
		error = 1;
	} else {
		lmip = atoi(argv[1]);
		lport = atoi(argv[2]);
		filename = argv[3];

		if((error = checknum(lmip, argv[1], 1, 254)) != 0) sprintf(errmsg, "Local MIP must be an integer between 1 and 254. %s is invalid.", argv[1]);
		else if((error = checknum(lport, argv[2], 1, 65535)) != 0) sprintf(errmsg, "Port number must be an integer between 1 and 65535. %s is invalid.", argv[2]);
	}

	if(error) {
		printf("%s: error: %s\n", argv[0], errmsg);
		printf("%s: usage: %s\n", argv[0], usemsg);
		free(errmsg);
		free(usemsg);
		return 0;
	} else{
		free(errmsg);
		free(usemsg);
		return 1;
	}

}

/**
 * Checks a given number agains a given string, to see if a 0 returned from atoi means a 0, or an invalid input
 * @param  check Number to check
 * @param  arg   String to check against
 * @param  lval  Lower limit of accepted value
 * @param  uval  Upper limit of accepted value
 * @return       0 if OK, 1 if not
 */
int checknum(int check, char *arg, int lval, int uval) {
	if(check < lval || check > uval) return 1;
	else if(check == 0 && strcmp(arg, "0") != 0) return 1;
	else return 0;
}