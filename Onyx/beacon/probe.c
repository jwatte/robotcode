
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <signal.h>

unsigned short port = 10169;

static char const *ipaddr(struct sockaddr_in const *sin) {
	static char ret[32];
	unsigned char const *addr = (unsigned char const *)&sin->sin_addr;
	sprintf(ret, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	return ret;
}

int sock = -1;

static void alarm_handler(int sig) {
	close(sock);
}

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
	struct sockaddr_in sin = { 0 };
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
    memset(&sin.sin_addr, 0xff, sizeof(sin.sin_addr));
	int w = sendto(sock, "discover", 8, 0, (struct sockaddr *)&sin, sizeof(sin));
	if (w != 8) {
		perror("sendto");
		exit(2);
	}
	char buf[1024];
	socklen_t slen = sizeof(sin);
	signal(SIGALRM, &alarm_handler);
	alarm(1);
	int r = recvfrom(sock, buf, 1024, 0, (struct sockaddr *)&sin, &slen);
	if (r == 4 && !strncmp(buf, "here", 4)) {
		fprintf(stdout, "%s\n", ipaddr(&sin));
	}
	else if (r < 0) {
        int rn = errno;
        if (rn == 9) {  //  no such socket -- alarm
            goto again;
        }
		perror("recvfrom");
		exit(2);
	}
	else {
		fprintf(stderr, "unknown response from %s\n", ipaddr(&sin));
		exit(2);
	}
	return 0;
}

