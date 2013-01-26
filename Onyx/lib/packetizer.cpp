
#include "inetwork.h"
#include "istatus.h"
#include <string.h>
#include <stdexcept>


enum {
    MAX_PACKET_BUFFER = 8192
};

class Packetizer : public IPacketizer {
public:
    Packetizer(INetwork *network, IStatus *status);
    virtual void step();
    virtual bool receive(unsigned char &code, size_t &size, void const *&data);
    virtual void broadcast(unsigned char code, size_t size, void const *data);
    virtual void respond(unsigned char code, size_t size, void const *data);
    virtual void vrespond(unsigned char code, size_t count, iovec const *vecs);
    
    void flush_bbuf();
    void flush_rbuf();
    size_t read_sz();
    size_t write_sz(size_t size, unsigned char *buf);

    INetwork *inet_;
    IStatus *istatus_;
    unsigned char const *inPtr_;
    unsigned char const *inEnd_;
    unsigned char b_buffer_[MAX_PACKET_BUFFER];
    unsigned char *b_outPtr_;
    unsigned char r_buffer_[MAX_PACKET_BUFFER];
    unsigned char *r_outPtr_;
};


Packetizer::Packetizer(INetwork *network, IStatus *status) :
    inet_(network),
    istatus_(status),
    inPtr_(0),
    inEnd_(0),
    b_outPtr_(b_buffer_),
    r_outPtr_(r_buffer_) {
    memset(b_buffer_, 0xfc, sizeof(b_buffer_));
    memset(r_buffer_, 0xfc, sizeof(r_buffer_));
}

void Packetizer::step() {
    flush_bbuf();
    flush_rbuf();
    inet_->step();
}

void Packetizer::flush_bbuf() {
    if (b_outPtr_ != b_buffer_) {
        inet_->broadcast(b_outPtr_ - b_buffer_, b_buffer_);
        b_outPtr_ = b_buffer_;
    }
}

void Packetizer::flush_rbuf() {
    if (r_outPtr_ != r_buffer_) {
        inet_->respond(r_outPtr_ - r_buffer_, r_buffer_);
        r_outPtr_ = r_buffer_;
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


void Packetizer::broadcast(unsigned char code, size_t size, void const *data) {
    unsigned char hdr[10];
    hdr[0] = code;
    size_t hsz = 1 + write_sz(size, &hdr[1]);
    if (hsz + size > (size_t)(sizeof(b_buffer_) - (b_outPtr_ - b_buffer_))) {
        //  If I can't fit it into available space, flush whatever I've sent so far.
        flush_bbuf();
    }
    if (hsz + size > (size_t)(sizeof(b_buffer_) - (b_outPtr_ - b_buffer_))) {
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
        memcpy(b_outPtr_, hdr, hsz);
        memcpy(b_outPtr_ + hsz, data, size);
        b_outPtr_ += hsz + size;
    }
}

void Packetizer::respond(unsigned char code, size_t size, void const *data) {
    iovec iov;
    memset(&iov, 0, sizeof(iov));
    iov.iov_base = const_cast<void *>(data);
    iov.iov_len = size;
    vrespond(code, 1, &iov);
}

void Packetizer::vrespond(unsigned char code, size_t count, iovec const *vecs) {
    if (count > 9) {
        throw std::runtime_error("Too many iovecs in operation");
    }
    iovec iov[10];
    size_t size = 0;
    for (size_t i = 0; i < count; ++i) {
        size += vecs[i].iov_len;
        iov[i + 1] = vecs[i];
    }
    unsigned char hdr[10];
    hdr[0] = code;
    size_t hsz = 1 + write_sz(size, &hdr[1]);
    if (hsz + size > (size_t)(sizeof(r_buffer_) - (r_outPtr_ - r_buffer_))) {
        //  If I can't fit it into available space, flush whatever I've sent so far.
        flush_rbuf();
    }
    if (hsz + size > (size_t)(sizeof(r_buffer_) - (r_outPtr_ - r_buffer_))) {
        //  If I still cannot fit it into the buffer, send it as its own packet.
        iov[0].iov_base = hdr;
        iov[0].iov_len = hsz;
        inet_->vsend(true, 1 + count, iov);
    }
    else {
        //  accumulate data into the outgoing buffer.
        for (size_t i = 0; i <= count; ++i) {
            memcpy(r_outPtr_, iov[i].iov_base, iov[i].iov_len);
            r_outPtr_ += iov[i].iov_len;
        }
    }
}

IPacketizer *packetize(INetwork *net, IStatus *status) {
    return new Packetizer(net, status);
}
