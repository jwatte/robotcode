
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

unsigned short port = 10169;

struct sockaddr_in found[20];
int nfound;

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

void usage() {
    fprintf(stderr, "usage: beacon [serve | scan]\n");
    fprintf(stderr, "serve is default; port is %d\n", port);
    exit(1);
}

int makesocket(unsigned short portarg, struct sockaddr_in *sin) {
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
    memset(sin, 0, sizeof(*sin));
	sin->sin_family = AF_INET;
	sin->sin_port = htons(portarg);
	err = bind(sock, (struct sockaddr *)sin, sizeof(*sin));
	if (err < 0) {
		perror("bind");
		exit(1);
	}
    return sock;
}

static inline long long myclock()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000LL + tv.tv_usec / 1000);
}

void do_scan() {
	struct sockaddr_in sin = { 0 };
    int sock = makesocket(0, &sin);
    fd_set fds;
    memset(&sin.sin_addr, 0xff, sizeof(sin.sin_addr));
    sin.sin_port = htons(port);
    long long ms = myclock() + 1500;
    long long left;
    struct timeval tv = { 0 };
    while ((left = ms - myclock()) > 0) {
        int r = sendto(sock, "discover", 8, 0, 
            (struct sockaddr *)&sin, sizeof(sin));
        if (r < 0) {
            perror("sendto");
            exit(1);
        }
    again:
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        r = select(sock+1, &fds, 0, 0, &tv);
        if (r <= 0) {
            break;
        }
        char buf[1024];
        struct sockaddr_in from;
        socklen_t slen = sizeof(from);
        r = recvfrom(sock, buf, 1024, 0, 
            (struct sockaddr *)&from, &slen);
        if (r > 0) {
            if (r == 8 && !strncmp("discover", buf, 8)) {
                //  don't re-send just because I got (my own?) discover
                goto again;
            }
            for (int i = 0; i < nfound; ++i) {
                if (!memcmp(&found[i].sin_addr, &from.sin_addr, sizeof(from.sin_addr))) {
                    //  already printed this guy
                    goto again;
                }
            }
            fprintf(stdout, "%s: %.*s\n", ipaddr(&from), r, buf);
            found[nfound++] = from;
            if (nfound == sizeof(found)/sizeof(found[0])) {
                //  found enough different addresses!
                break;
            }
        }
    }
    fprintf(stderr, "found %d addresses\n", nfound);
}

int main(int argc, char const *argv[]) {

    if (argc > 2) {
        usage();
    }
    if (argc == 2) {
        if (!strcmp(argv[1], "serve")) {
            goto rebind;
        }
        else if (!strcmp(argv[1], "scan")) {
            do_scan();
            return 0;
        }
        else {
            usage();
        }
    }

rebind:
	signal(SIGALRM, &on_alarm);
	struct sockaddr_in sin = { 0 };
    int sock = makesocket(port, &sin);
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
            fprintf(stderr, "response to %s\n", ipaddr(
                (struct sockaddr_in const *)&from));
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

