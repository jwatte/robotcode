
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

unsigned short port = 10169;

int main() {
	int sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}
	int one = 1;
	int err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
	if (err < 0) {
		perror("sockopt broadcast");
		exit(1);
	}
	struct sockaddr_in sin = { 0 };
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	err = bind(sock, (struct sockaddr *)&sin, sizeof(sin));
	if (err < 0) {
		perror("bind");
		exit(1);
	}

	while (1) {
		struct sockaddr from;
		char buf[1024];
		socklen_t slen = sizeof(from);
		int r = recvfrom(sock, buf, 1024, 0, &from, &slen);
		if (r == 8 && !strncmp(buf, "discover", 8)) {
			r = sendto(sock, "here", 4, 0, &from, slen);
			if (r < 0) {
				perror("sendto");
			}
		}
		else if (r < 0) {
			perror("recvfrom");
			exit(1);
		}
	}

	return 0;
}

