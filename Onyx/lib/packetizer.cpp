
#include "inetwork.h"
#include "istatus.h"
#include <string.h>


enum {
    MAX_PACKET_BUFFER = 8192
};

class Packetizer : public IPacketizer {
public:
    Packetizer(INetwork *network, IStatus *status);
    virtual void step();
    virtual bool receive(unsigned char &code, size_t &size, void const *&data);
    virtual void send(unsigned char code, size_t size, void const *data);
    
    void flush_buf();
    size_t read_sz();
    size_t write_sz(size_t size, unsigned char *buf);

    INetwork *inet_;
    IStatus *istatus_;
    unsigned char const *inPtr_;
    unsigned char const *inEnd_;
    unsigned char buffer_[MAX_PACKET_BUFFER];
    unsigned char *outPtr_;
};


Packetizer::Packetizer(INetwork *network, IStatus *status) :
    inet_(network),
    istatus_(status),
    inPtr_(0),
    inEnd_(0),
    outPtr_(buffer_) {
    memset(buffer_, 0xfc, sizeof(buffer_));
}

void Packetizer::step() {
    flush_buf();
    inet_->step();
}

void Packetizer::flush_buf() {
    if (outPtr_ != buffer_) {
        inet_->send(outPtr_ - buffer_, buffer_);
        outPtr_ = buffer_;
    }
}

bool Packetizer::receive(unsigned char &code, size_t &size, void const *&data) {
    code = 0;
    size = 0;
    data = 0;
    if (inPtr_ == inEnd_) {
read_a_packet:
        size_t s = 0;
        void const *d = 0;
        if (!inet_->receive(s, d)) {
            return false;
        }
        inPtr_ = reinterpret_cast<unsigned char const *>(d);
        inEnd_ = inPtr_ + s;
    }
    if (inEnd_ - inPtr_ < 2) {
        istatus_->error("Network message with junk byte at end.");
        goto read_a_packet;
    }
    code = *inPtr_;
    ++inPtr_;
    size = read_sz();
    if (inEnd_ - inPtr_ < (ssize_t)size) {
        istatus_->error("Network message truncated at end.");
        goto read_a_packet;
    }
    data = inPtr_;
    inPtr_ += size;
    return true;
}

//  read a vari-length encoded unsigned integer
size_t Packetizer::read_sz() {
    size_t ret = 0;
    while (inPtr_ != inEnd_) {
        ret = (ret << 7) | (*inPtr_ & 0x7f);
        ++inPtr_;
        if (!(inPtr_[-1] & 0x80)) {
            break;
        }
    }
    return ret;
}

size_t Packetizer::write_sz(size_t size, unsigned char *buf) {
    size_t sz = size;
    size_t c = 1;
    while (sz > 127) {
        ++c;
        sz = sz >> 7;
    }
    size_t ret = c;
    sz = size;
    while (c > 1) {
        --c;
        *buf++ = ((sz >> (7 * c)) & 0x7f) | 0x80;
        sz = sz & ((1 << (7 * c)) - 1);
    }
    *buf++ = (unsigned char)(sz & 0x7f);
    return ret;
}


void Packetizer::send(unsigned char code, size_t size, void const *data) {
    unsigned char hdr[10];
    hdr[0] = code;
    size_t hsz = 1 + write_sz(size, &hdr[1]);
    if (hsz + size > (size_t)(sizeof(buffer_) - (outPtr_ - buffer_))) {
        //  If I can't fit it into available space, flush whatever I've sent so far.
        flush_buf();
    }
    if (hsz + size > (size_t)(sizeof(buffer_) - (outPtr_ - buffer_))) {
        //  If I still cannot fit it into the buffer, send it as its own packet.
        iovec iov[2];
        iov[0].iov_base = hdr;
        iov[0].iov_len = hsz;
        iov[1].iov_base = const_cast<void *>(data);
        iov[1].iov_len = size;
        inet_->vsend(false, 2, iov);
    }
    else {
        //  accumulate data into the outgoing buffer.
        memcpy(outPtr_, hdr, hsz);
        memcpy(outPtr_ + hsz, data, size);
        outPtr_ += hsz + size;
    }
}

IPacketizer *packetize(INetwork *net, IStatus *status) {
    return new Packetizer(net, status);
}
