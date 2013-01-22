
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

typedef enum {
    false, true, FileNotFound   //  :-)
} bool;


static char const *ipaddr(struct sockaddr_in const *sin) {
	static char ret[32];
	unsigned char const *addr = (unsigned char const *)&sin->sin_addr;
	sprintf(ret, "%d.%d.%d.%d", addr[0], addr[1], addr[2], addr[3]);
	return ret;
}

int sock = -1;
bool once = false;
bool quiet = false;
bool verbose = false;

int main(int argc, char const *argv[]) {

    while (argv[1]) {
        if (!strcmp(argv[1], "-1")) {
            once = true;
        }
        else if (!strcmp(argv[1], "-q")) {
            quiet = true;
            verbose = false;
        }
        else if (!strcmp(argv[1], "-v")) {
            verbose = true;
            quiet = false;
        }
        else {
            fprintf(stderr, "usage: probe [-qv1]\n");
            exit(1);
        }
        ++argv;
        --argc;
    }

again:
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
        if (!quiet) {
            perror("socket");
        }
		exit(1);
	}
    if (verbose) {
        fprintf(stderr, "socket %d\n", sock);
    }
	int one = 1;
	int err = setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one));
	if (err < 0) {
        if (!quiet) {
            perror("sockopt broadcast");
        }
		exit(1);
	}
    for (int i = 0; i < 10; ++i) {
        struct sockaddr_in sin = { 0 };
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        memset(&sin.sin_addr, 0xff, sizeof(sin.sin_addr));
        int w = sendto(sock, "discover", 8, 0, (struct sockaddr *)&sin, sizeof(sin));
        if (w != 8) {
            if (!quiet) {
                perror("sendto");
            }
            exit(2);
        }
        if (verbose) {
            fprintf(stderr, "sendto %d\n", w);
        }
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;
        while (select(sock+1, &fds, 0, 0, &tv) > 0) {
            if (verbose) {
                fprintf(stderr, "select\n");
            }
            char buf[1024];
            socklen_t slen = sizeof(sin);
            int r = recvfrom(sock, buf, 1024, 0, (struct sockaddr *)&sin, &slen);
            if (verbose) {
                fprintf(stderr, "recvfrom %d\n", r);
            }
            if (r == 4 && !strncmp(buf, "here", 4)) {
                fprintf(stdout, "%s\n", ipaddr(&sin));
                if (once) {
                    exit(0);
                }
            }
            else if (r < 0) {
                if (!quiet) {
                    perror("recvfrom");
                }
                exit(2);
            }
            else {
                if (!quiet) {
                    fprintf(stderr, "unknown response from %s\n", ipaddr(&sin));
                }
            }
        }
    }
    close(sock);
    sock = -1;
    goto again;
}

