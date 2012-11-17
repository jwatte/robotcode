
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/signal.h>
#include <time.h>

#include <openssl/md5.h>

#include "../lib/defs.h"


char const *myname;
bool verbose = true;

volatile bool interrupted;

void onintr(int) {
    interrupted = true;
}


double now() {
    struct timespec tspec;
    clock_gettime(CLOCK_MONOTONIC, &tspec);
    return (double)tspec.tv_sec + (double)tspec.tv_nsec * 1.0e-9;
}

void getaddr(sockaddr_in const *from, char *obuf) {
    unsigned char const *ptr = (unsigned char const *)&from->sin_addr;
    unsigned short port = htons(from->sin_port);
    sprintf(obuf, "%d.%d.%d.%d:%d", ptr[0], ptr[1], ptr[2], ptr[3], port);
}



int sock;

void init_socket() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket()");
        exit(10);
    }
    int so = 1;
    socklen_t olen = sizeof(so);
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &so, olen) < 0) {
        perror("SO_REUSEADDR");
        exit(11);
    }
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &so, olen) < 0) {
        perror("SO_BROADCAST");
        exit(11);
    }
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(CONTROL_PORT);
    if (bind(sock, (sockaddr const *)&sin, sizeof(sin)) < 0) {
        perror("bind()");
        exit(11);
    }
}

//  Make really sure that this packet is intended for my 
//  particular protocol.
bool verify_csum(void const *packet, size_t size) {
    if (size < 5) {
        return false;
    }
    unsigned char save[4];
    memcpy(save, packet, 4);
    ((char *)packet)[0] = 'r';
    ((char *)packet)[1] = 'o';
    ((char *)packet)[2] = 'b';
    ((char *)packet)[3] = 'o';
    unsigned char dig[16];
    memset(dig, 0, sizeof(dig));
    MD5((unsigned char const *)packet, size, dig);
    if (memcmp(save, dig, 4)) {
        //  checksum failed
        return false;
    }
    return true;
}

char spacket[65536];

void do_send(void const *buf, size_t size, sockaddr_in const *to) {
    if (size > sizeof(spacket) - 4) {
        fprintf(stderr, "too long packet (%d bytes) in do_send()\n", (int)size);
        exit(100);
    }
    memcpy(&spacket[4], buf, size);
    spacket[0] = 'r';
    spacket[1] = 'o';
    spacket[2] = 'b';
    spacket[3] = 'o';
    unsigned char sig[16];
    memset(sig, 0, sizeof(sig));
    MD5((unsigned char const *)spacket, size + 4, sig);
    memcpy(spacket, sig, 4);
    int s = ::sendto(sock, spacket, size + 4, 0, (sockaddr const *)to, sizeof(sockaddr_in));
    if (s < 0) {
        fprintf(stderr, "sendto() of %d bytes failed.\n", (int)size + 4);
        exit(100);
    }
}



void handle_pong(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    char buf[256];
    memcpy(buf, &((cmd_pong *)hdr)[1], ((cmd_pong *)hdr)->slen);
    buf[((cmd_pong *)hdr)->slen] = 0;
    if (verbose) {
        fprintf(stderr, "got pong from %s\n", buf);
    }
}

struct cmd_handler {
    unsigned char cmd;
    size_t min_size;
    void (*handle)(packet_hdr const *hdr, size_t size, sockaddr_in const *from);
};

cmd_handler handlers[] = {
    { cPong, sizeof(cmd_pong), &handle_pong },
};

void dispatch_packet(void const *packet, size_t size, sockaddr_in const *from) {
    char ipaddr[30];
    getaddr(from, ipaddr);
    if (verbose) {
        fprintf(stderr, "got packet from %s\n", ipaddr);
    }
    if (!verify_csum(packet, size)) {
        //  not intended for my kind
        fprintf(stderr, "packet checksum was wrong from %s\n", ipaddr);
        return;
    }
    packet_hdr const *hdr = (packet_hdr const *)((char const *)packet + 4);
    for (size_t i = 0, n = sizeof(handlers)/sizeof(handlers[0]); i != n; ++i) {
        if (handlers[i].cmd == hdr->cmd) {
            if (size >= handlers[i].min_size) {
                if (verbose) {
                    fprintf(stderr, "handling packet %d from %s\n", hdr->cmd, ipaddr);
                }
                (*handlers[i].handle)(hdr, size, from);
                return;
            }
            else {
                //  got packet from different version
                fprintf(stderr, "packet was too short from %s\n", ipaddr);
                break;
            }
        }
    }
    if (verbose) {
        fprintf(stderr, "packet type %d from %s not recognized\n", hdr->cmd, ipaddr);
    }
}

char dpacket[65536];
int dpacketlen;

void recv_one() {
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    socklen_t slen = sizeof(sin);
    dpacketlen = ::recvfrom(sock, dpacket, sizeof(dpacket), 0, (sockaddr *)&sin, &slen);
    if (dpacketlen > 0) {
        dispatch_packet(dpacket, dpacketlen, &sin);
    }
}

void poll_socket() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval tv = { 0, 10000 };
    if (select(sock + 1, &fds, 0, 0, &tv) > 0) {
        recv_one();
    }
}

double last_request = 0;
double last_ping = 0;

void commander_worker() {
    double n = now();
    if (n - last_request > 0.5) {
        //  request reports
        //  request images
        last_request = n;
    }
    if (n - last_ping > 2) {
        char buf[256];
        cmd_pong *cp = (cmd_pong *)buf;
        cp->cmd = cPing;
        cp->slen = strlen(myname);
        memcpy(&cp[1], myname, cp->slen);
        struct sockaddr_in bc;
        memset(&bc, 0, sizeof(bc));
        memset(&bc.sin_addr, 0xff, sizeof(bc.sin_addr));
        bc.sin_port = htons(CONTROL_PORT);
        do_send(buf, sizeof(*cp) + cp->slen, &bc);
        last_ping = n;
    }
}

int main(int argc, char const *argv[]) {
    if (argc > 1) {
        myname = argv[1];
        ++argv;
        --argc;
    }
    else {
        myname = getenv("CMDNAME");
    }
    if (!myname) {
        fprintf(stderr, "specify name as first argument, or CMDNAME environment variable.\n");
        exit(1);
    }
    if (strlen(myname) > 64) {
        fprintf(stderr, "too long commander name: %d\n", (int)strlen(myname));
        exit(1);
    }
    if (strlen(myname) < 1) {
        fprintf(stderr, "commander name cannot be empty.\n");
        exit(1);
    }

    signal(SIGINT, onintr);
    init_socket();

    while (!interrupted) {
        poll_socket();
        commander_worker();
    }

    return 0;
}

