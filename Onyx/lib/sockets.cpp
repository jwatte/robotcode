
#include "inetwork.h"
#include "istatus.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>


class Sockets : public ISockets {
public:
    unsigned short port_;
    int fd_;
    IStatus *status_;

    Sockets(unsigned short port, IStatus *status) :
        port_(port),
        fd_(-1),
        status_(status) {
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd_ < 0) {
            status->error("could not open socket()");
            return;
        }

        int one = 1;
        socklen_t slen = sizeof(one);
        int err = setsockopt(fd_, SOL_SOCKET, SO_BROADCAST, &one, slen);
        if (err < 0) {
            status->error("set socket broadcast enable failed");
            ::close(fd_);
            fd_ = -1;
            return;
        }

        one = 1024*1024;
        slen = sizeof(one);
        err = setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &one, slen);
        if (err < 0) {
            status->error("set socket send buffer size failed");
            close(fd_);
            fd_ = -1;
            return;
        }

        one = 1024*1024;
        slen = sizeof(one);
        err = setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &one, slen);
        if (err < 0) {
            status->error("set socket receive buffer size failed");
            close(fd_);
            fd_ = -1;
            return;
        }

        sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        err = bind(fd_, (struct sockaddr const *)&sin, sizeof(sin));
        if (err < 0) {
            status->error("socket bind address failed");
            close(fd_);
            fd_ = -1;
            return;
        }

        err = fcntl(fd_, F_SETFL, O_NONBLOCK);
        if (err < 0) {
            status->error("socket set non-blocking failed");
            close(fd_);
            fd_ = -1;
            return;
        }
    }

    bool ok() {
        return fd_ >= 0;
    }

    virtual int recvfrom(void *buf, size_t sz, sockaddr_in &addr) {
        socklen_t slen = sizeof(addr);
        return ::recvfrom(fd_, buf, sz, 0, (sockaddr *)&addr, &slen);
    }

    virtual int sendto(void const *buf, size_t sz, sockaddr_in const &addr) {
        sockaddr_in sin(addr);
        if (sin.sin_port == 0) {
            sin.sin_port = htons(port_);
        }
        int s = ::sendto(fd_, buf, sz, 0, (sockaddr const *)&sin, sizeof(sin));
        return s;
    }

};


ISockets *mksocks(unsigned short port, IStatus *status) {
    Sockets *socks = new Sockets(port, status);
    if (!socks->ok()) {
        delete socks;
        return 0;
    }
    return socks;
}

