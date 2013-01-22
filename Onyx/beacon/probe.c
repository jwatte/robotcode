
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>
#include <sys/select.h>

unsigned short port = 10169;

static char const *ipaddr(struct sockaddr_in const *sin) {
	static char ret[32];
	unsigned char const *addr = (unsigned char const *)&sin->sin_addr;
	sprintf(ret, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	return ret;
}

int sock = -1;

int main() {

again:
	sock = socket(AF_INET, SOCK_DGRAM, 0);
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
    for (int i = 0; i < 10; ++i) {
        struct sockaddr_in sin = { 0 };
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        memset(&sin.sin_addr, 0xff, sizeof(sin.sin_addr));
        int w = sendto(sock, "discover", 8, 0, (struct sockaddr *)&sin, sizeof(sin));
        if (w != 8) {
            perror("sendto");
            exit(2);
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        while (select(sock+1, &fds, 0, 0, &tv) > 0) {
            char buf[1024];
            socklen_t slen = sizeof(sin);
            int r = recvfrom(sock, buf, 1024, 0, (struct sockaddr *)&sin, &slen);
            if (r == 4 && !strncmp(buf, "here", 4)) {
                fprintf(stdout, "%s\n", ipaddr(&sin));
            }
            else if (r < 0) {
                perror("recvfrom");
                exit(2);
            }
            else {
                fprintf(stderr, "unknown response from %s\n", ipaddr(&sin));
            }
        }
    }
    goto again;
	return 0;
}

