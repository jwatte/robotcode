
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

unsigned short port = 10169;

static char const *ipaddr(struct sockaddr_in const *sin) {
	static char ret[32];
	unsigned char const *addr = (unsigned char const *)&sin->sin_addr;
	sprintf(ret, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	return ret;
}

volatile int alarmed = 0;

void on_alarm(int sig) {
	alarmed = 1;
}

int main() {

rebind:
	signal(SIGALRM, &on_alarm);
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
	memset(&sin.sin_addr, 0xff, sizeof(sin.sin_addr));
	int r = sendto(sock, "hereami", 7, 0, 
		(struct sockaddr *)&sin, sizeof(sin));
	if (r < 0) {
		perror("sendto");
		exit(1);
	}

	while (1) {
		struct sockaddr from;
		char buf[1024];
		socklen_t slen = sizeof(from);
		alarm(400);
		int r = recvfrom(sock, buf, 1024, 0, &from, &slen);
		if (r == 8 && !strncmp(buf, "discover", 8)) {
			r = sendto(sock, "here", 4, 0, &from, slen);
			if (r < 0) {
				perror("sendto");
			}
		}
		else if (r < 0) {
			if (!alarmed) {
				perror("recvfrom");
				exit(1);
			}
			//	rebind, in case a new interface came up
			alarmed = 0;
			close(sock);
			sock = -1;
			goto rebind;
		}
        else {
            if (r > 30) {
                r = 30;
            }
            buf[r] = 0;
            fprintf(stderr, "unknown msg: %s from %s\n", buf, ipaddr(
                (struct sockaddr_in const *)&from));
        }
	}

	return 0;
}

