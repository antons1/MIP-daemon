#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <poll.h>
#include <sys/time.h>

#include <sys/socket.h>
#include <sys/un.h>

#define FID 1

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
	if(argc < 2) {
		printf("%s: error: too few arguments\n", argv[0]);
		printf("%s: usage: %s [local MIP address]\n", argv[0], argv[0]);
	}

	unsigned char localmip = (unsigned char)atoi(argv[1]);
	printf("Waiting for PINGs on MIP %d\n", localmip);

	int fd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
	if(fd == -1) {
		perror("Error creating socket");
		return 1;
	}

	struct sockaddr_un local;
	local.sun_family = AF_UNIX;
	sprintf(local.sun_path, "socket%d", localmip);

	if(connect(fd, (struct sockaddr *)&local, sizeof(struct sockaddr_un)) == -1) {
		perror("Error connection to socket");
		return 1;
	}

	struct us_frame *id = malloc(sizeof(struct us_frame));
	memset(id, 0, sizeof(struct us_frame));

	id->dst_addr = localmip;
	id->mode = 1;
	id->padding = FID;

	if(send(fd, (char *)id, sizeof(struct us_frame), 0) == -1) {
		perror("Error identifying to daemon");
		free(id);
		return 1;
	}

	free(id);

	struct pollfd fds[1];

	fds[0].fd = fd;
	fds[0].events = POLLIN | POLLHUP;

	while(1) {
		int rc = poll(fds, 1, 0);
		if(rc == -1) perror("Error polling fd");

		if(fds[0].revents & POLLHUP) {
			printf("Daemon broke connection\n");
			close(fds[0].fd);
			return 0;
		}

		if(fds[0].revents & POLLIN) {
			char buf[1500];
			memset(buf, 0, 1500);
			recv(fds[0].fd, buf, 1500, 0);

			struct us_frame *packet = (struct us_frame *)buf;

			printf("Recieved PING from %d: %s\n", packet->src_addr, packet->content);

			struct us_frame *response = malloc(sizeof(struct us_frame)+strlen("PONG")+1);
			memset(response, 0, sizeof(struct us_frame)+strlen("PONG")+1);
			response->src_addr = localmip;
			response->dst_addr = packet->src_addr;
			response->mode = 1;
			strcpy(response->content, "PONG");

			send(fds[0].fd, (char *)response, sizeof(struct us_frame)+strlen("PONG")+1, 0);

			free(response);
		}
	}
	return 0;
}