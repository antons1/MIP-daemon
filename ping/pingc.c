#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/un.h>

#define FID 2

struct us_frame {
	uint8_t src_addr:8;
	uint8_t dst_addr:8;
	uint8_t mode:1;
	uint8_t padding:7;
	char content[0];
} __attribute__ ((packed));

/**
 * Main
 * @param  argc Argument count
 * @param  argv Arguments
 * @return      0 on success, something else on fail
 */
int main(int argc, char *argv[]) {
	if(argc < 4) {
		printf("%s: error: too few arguments\n", argv[0]);
		printf("%s: usage: %s [Local MIP address] [Destination MIP address] [Message]\n", argv[0], argv[0]);
	}

	unsigned char localmip = (unsigned char)atoi(argv[1]);
	unsigned char dstmip = (unsigned char)atoi(argv[2]);
	char *msg = argv[3];

	printf("Sending message \"%s\" from %d to %d\n", msg, localmip, dstmip);

	int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);

	if(fd == -1) {
		perror("Error creating socket");
		return 0;
	}

	struct sockaddr_un local;
	local.sun_family = AF_UNIX;
	sprintf(local.sun_path, "socket%d", localmip);

	if(connect(fd, (struct sockaddr *)&local, sizeof(struct sockaddr_un)) == -1) {
		perror("Connection to local socket failed");
		return 1;
	}

	struct us_frame *uframe = malloc(sizeof(struct us_frame)+strlen(msg)+1);
	memset(uframe, 0, sizeof(struct us_frame)+strlen(msg)+1);
	uframe->src_addr = localmip;
	uframe->dst_addr = dstmip;
	uframe->mode = 0;
	strcpy(uframe->content, msg);
	uframe->padding = FID;

	if(send(fd, (char *)uframe, sizeof(struct us_frame)+strlen(msg)+1, 0) == -1) {
		perror("Error identifying to daemon");
		free(uframe);
		close(fd);
		return 1;
	}

	uframe->padding = 0;

	if(send(fd, (char *)uframe, sizeof(struct us_frame)+strlen(msg)+1, 0) == -1) {
		perror("Error sending PING message");
		free(uframe);
		close(fd);
		return 1;
	}

	struct pollfd fds[1];
	fds[0].fd = fd;
	fds[0].events = POLLIN | POLLHUP;

	struct timeval *sendtime = malloc(sizeof(struct timeval));
	gettimeofday(sendtime, NULL);

	int rc = poll(fds, 1, 1000);

	if(rc == -1) {
		perror("Error polling file descriptor");
	} else if(rc == 0) {
		printf("Timeout\n");
	} else {
		if(fds[0].revents & POLLHUP) {
			printf("Daemon disconnected\n");
		}

		if(fds[0].revents & POLLIN) {
			char buf[1500];
			memset(buf, 0, 1500);
			ssize_t read = recv(fds[0].fd, buf, 1500, 0);
			if(read == 0) return 1;
			struct us_frame *uf = (struct us_frame*)buf;
			struct timeval *recievetime = malloc(sizeof(struct timeval));
			gettimeofday(recievetime, NULL);

			int sec = (int)(recievetime->tv_sec-sendtime->tv_sec);
			int usec = (int)(recievetime->tv_usec-sendtime->tv_usec)/1000;

			if(sec > 0) {
				sec--;
				usec += 1000;
			}

			printf("Recieved answer from %d, in %ds %dms: %s\n", uf->src_addr, sec, usec, uf->content);

			free(recievetime);
		}
	}

	free(uframe);
	free(sendtime);
	close(fd);

	return 0;
}