
#include "inetwork.h"
#include "istatus.h"
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>
#include <stdexcept>



class TCPSocket : public ISocket {
public:
    TCPSocket(sockaddr_in const &sin, IStatus *status) :
        addr_(sin),
        status_(status),
        fd_(-1),
        sndpos_(0),
        sndend_(0),
        rcvpos_(0),
        rcvend_(0) {
        fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (fd_ < 0) {
            status->error("could not open socket()");
            return;
        }

        int one = 1024*1024;
        socklen_t slen = sizeof(one);
        int err = setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &one, slen);
        if (err < 0) {
            status->error("set socket send buffer size failed");
            close(fd_);
            fd_ = -1;
            return;
        }

        one = 1024*1024*2;
        slen = sizeof(one);
        err = setsockopt(fd_, SOL_SOCKET, SO_RCVBUF, &one, slen);
        if (err < 0) {
            status->error("set socket receive buffer size failed");
            close(fd_);
            fd_ = -1;
            return;
        }

        err = ::connect(fd_, (sockaddr const *)&addr_, sizeof(addr_));
        if (err < 0) {
            int en = errno;
            status->error(std::string("Error connecting socket: ") + strerror(en));
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

    ~TCPSocket() {
        if (fd_ != -1) {
            ::close(fd_);
            fd_ = -1;
        }
    }

    //  like recv(), but keep the data in the incoming buffer.
    virtual size_t peek(void *buf, size_t maxSize) {
        if (fd_ == -1) {
            return 0;
        }
        if (rcvend_ > rcvpos_) {
            if (maxSize > rcvend_ - rcvpos_) {
                maxSize = rcvend_ - rcvpos_;
            }
        }
        else {
            maxSize = 0;
        }
        if (maxSize > 0) {
            memcpy(buf, &rcvbuf_[rcvpos_], maxSize);
        }
        return maxSize;
    }

    //  recv just receives from the big outgoing buffer
    virtual void recvd(size_t maxSize) {
        rcvpos_ += maxSize;
        if (rcvpos_ > rcvend_) {
            status_->error("bad call to recvd(): past end of buffer");
            throw std::runtime_error("bad recvd() call");
        }
    }

    //  send just copies into the big outgoing buffer
    virtual size_t send(void const *buf, size_t size) {
        if (fd_ == -1) {
            return 0;
        }
        if (sizeof(sndbuf_) - sndend_ < size) {
            if (sndpos_ > 0) {
                memmove(&sndbuf_[0], &sndbuf_[sndpos_], sndend_ - sndpos_);
                sndend_ -= sndpos_;
                sndpos_ = 0;
            }
            if (sizeof(sndbuf_) - sndend_ < size) {
                size = sizeof(sndbuf_) - sndend_;
            }
        }
        if (size > 0) {
            memmove(&sndbuf_[sndend_], buf, size);
            sndend_ += size;
        }
        return size;
    }

    virtual bool step() {
        int err;
        if (fd_ != -1) {
            if (sndend_ > sndpos_) {
                err = ::send(fd_, &sndbuf_[sndpos_], sndend_ - sndpos_, 0);
                if (err < 0) {
                    status_->error("socket full timeout: " + ipaddr(addr_));
                    ::close(fd_);
                    fd_ = -1;
                }
                else {
                    sndpos_ += err;
                }
            }
        }
        if (fd_ != -1) {
            if (rcvend_ == sizeof(rcvbuf_)) {
                if (rcvpos_ > 0) {
                    memmove(&rcvbuf_[0], &rcvbuf_[rcvpos_], rcvend_ - rcvpos_);
                    rcvend_ -= rcvpos_;
                    rcvpos_ = 0;
                }
            }
            if (rcvend_ < sizeof(rcvbuf_)) {
                err = ::recv(fd_, &rcvbuf_[rcvend_], sizeof(rcvbuf_)-rcvend_, 0);
                if (err < 0) {
                    if (errno != EAGAIN) {
                        status_->error("socket error: " + ipaddr(addr_));
                        ::close(fd_);
                        fd_ = -1;
                    }
                }
                else if (err == 0) {
                    status_->error("socket closed: " + ipaddr(addr_));
                    ::close(fd_);
                    fd_ = -1;
                }
                else {
                    rcvend_ += err;
                }
            }
        }
        return fd_ != -1;
    }

    sockaddr_in addr_;
    IStatus *status_;
    int fd_;
    size_t sndpos_;
    size_t sndend_;
    char sndbuf_[1024*1024*4];
    size_t rcvpos_;
    size_t rcvend_;
    char rcvbuf_[1024*1024*4];
};

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

        one = 1024*1024*2;
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

    virtual boost::shared_ptr<ISocket> connect(sockaddr_in const &addr) {
        return boost::shared_ptr<TCPSocket>(new TCPSocket(addr, status_));
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

