#include "inetwork.h"
#include "istatus.h"
#include "itime.h"
#include "util.h"

#include <unordered_map>
#include <list>
#include <vector>
#include <sstream>
#include <string>
#include <stdexcept>
#include <boost/shared_ptr.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>



enum {
    //  This is almost pessimal on old Ethernet with ~1480 bytes UDP payload.
    //  This is almost optimal on WiFi with ~2200 bytes UDP payload.
    //  This is suboptimal but not bad on new Ethernet with ~9k bytes UDP payload.
    MAX_FRAGMENT_SIZE = 2048
};

//  When a receiver or packet doesn't have activity for 
//  this amount of time, time it out.
static const double packet_timeout = 2.5;

struct fragment {
    explicit fragment(size_t sz = 0) {
        //  todo: re-use buffers rather than thrashing the allocator
        physSize_ = sz < MAX_FRAGMENT_SIZE ? MAX_FRAGMENT_SIZE : sz;
        buf_ = new unsigned char[physSize_];
        usedSize_ = 0;
        offset_ = 0;
    }
    ~fragment() {
        delete[] buf_;
    }
    //  received data starts at buf_ and extends to usedSize_.
    unsigned char *buf_;
    //  size of remaining data is usedSize_ - offset_
    size_t usedSize_;
    //  remaining data starts at buf_ + offset_
    size_t offset_;
    //  the actual allocated buffer extends to physSize_
    size_t physSize_;
protected:
    explicit fragment(fragment &frag) {
        buf_ = frag.buf_;
        frag.buf_ = 0;
        usedSize_ = frag.usedSize_;
        frag.usedSize_ = 0;
        offset_ = frag.offset_;
        frag.offset_ = 0;
        physSize_ = frag.physSize_;
        frag.physSize_ = 0;
    }
private:
    fragment(fragment const &);
    fragment &operator=(fragment const &);
};

struct fragment_collection {
    fragment_collection(unsigned short seq, double time, boost::shared_ptr<fragment> const &f) :
        seq_(seq),
        lastTime_(time) {
        fragments_.push_back(f);
    }
    unsigned short seq_;
    double lastTime_;
    std::vector<boost::shared_ptr<fragment>> fragments_;
};

struct receive_info {
    receive_info(sockaddr_in const &sin) : addr_(sin), lastTime_(0) {}
    sockaddr_in addr_;
    std::list<fragment_collection> fragments_;
    double lastTime_;
};

struct send_info {
    send_info(sockaddr_in const &sin) : addr_(sin), lastTime_(0), nextSeq_(0) {}
    sockaddr_in addr_;
    std::list<boost::shared_ptr<fragment>> fragments_;
    double lastTime_;
    unsigned short nextSeq_;
};

struct deliver_fragment : public fragment {
    deliver_fragment(sockaddr_in const &from, fragment &frag) :
        fragment(frag),
        from_(from) {
    }
    sockaddr_in from_;
};


std::string ipaddr(sockaddr_in const &sin) {
    unsigned char const * sa = (unsigned char const *)&sin.sin_addr;
    std::stringstream ss;
    ss << (int)sa[0] << "." << (int)sa[1] << "." << (int)sa[2] << "." << (int)sa[3];
    return ss.str();
}

namespace std {
    template<> class hash<sockaddr_in> {
    public:
        inline unsigned int operator()(sockaddr_in const &sin) const {
            return fnv2_hash(&sin.sin_addr, 4) ^ fnv2_hash(&sin.sin_port, 2);;
        }
    };
};

inline bool operator==(sockaddr_in const &a, sockaddr_in const &b) {
    return !memcmp(&a.sin_addr, &b.sin_addr, sizeof(a.sin_addr)) &&
        a.sin_port == b.sin_port;
}
inline bool operator!=(sockaddr_in const &a, sockaddr_in const &b) {
    return !(a == b);
}

class Network : public INetwork {
public:
    virtual void step();
    virtual bool receive(size_t &size, void const *&packet);
    virtual void broadcast(size_t size, void const *packet);
    virtual void respond(size_t size, void const *packet);
    virtual void vsend(bool response, size_t count, iovec const *vecs);
    virtual void lock_address(double timeout);
    virtual void unlock_address();
    virtual bool is_locked();

    Network(ISockets *socks, ITime *time, IStatus *status, bool useB);
    ~Network();

private:

    void incoming_fragment(sockaddr_in const &from, boost::shared_ptr<fragment> &frag, double atTime);
    void check_senders(double now);
    void check_receivers(double now);
    void check_packets(double now);
    void complete_fragment(sockaddr_in const &from, boost::shared_ptr<fragment> &frag);
    void complete_fragment(sockaddr_in const &from, fragment_collection const &fc);
    void enqueue(sockaddr_in const &from, size_t count, iovec const *vecs);

    typedef std::unordered_map<sockaddr_in, boost::shared_ptr<receive_info>> recv_map;
    recv_map receivers_;
    ITime *time_;
    IStatus *status_;
    ISockets *socks_;
    double lastCheckTime_;
    //  to deliver to user
    std::list<boost::shared_ptr<deliver_fragment>> inqueue_;
    std::list<send_info> outqueue_;
    boost::shared_ptr<deliver_fragment> curFrag_;
    sockaddr_in remoteAddr_;
    bool locked_;
    bool broadcastOk_;
    double lockTimeout_;
    double lastLockReceiveTime_;
};


Network::Network(ISockets *socks, ITime *time, IStatus *status, bool canB) {
    socks_ = socks;
    time_ = time;
    status_ = status;
    lastCheckTime_ = 0;
    memset(&remoteAddr_, 0, sizeof(remoteAddr_));
    locked_ = false;
    broadcastOk_ = canB;
    lockTimeout_ = 0;
    lastLockReceiveTime_ = 0;

    status->message("network opened OK");
}

Network::~Network() {
}

void Network::step() {
    double now = time_->now();

    for (auto ptr(outqueue_.begin()), end(outqueue_.end()); ptr != end; ++ptr) {
        send_info &si(*ptr);
        bool error = false;
        while (!si.fragments_.empty()) {
            fragment &f(*si.fragments_.front());
            int s = socks_->sendto(f.buf_ + f.offset_, f.usedSize_ - f.offset_, si.addr_);
            if ((size_t)s != f.usedSize_ - f.offset_) {
                error = true;
                //  no use sending more to this guy
                break;
            }
            si.fragments_.pop_front();
        }
        if (error) {
            status_->error("send error to " + ipaddr(si.addr_));
        }
        si.fragments_.clear();
    }

    //  Drain the socket receive queue, but don't spin forever.
    //  The max buffer is 2048 kB as requested above (although Linux 
    //  will double that value "for book-keeping.")
    for (int i = 0; i < 1000; ++i) {
        sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        boost::shared_ptr<fragment> frag(new fragment());
        int r = socks_->recvfrom(frag->buf_, frag->physSize_, sin);
        if (r < 0) {
            break;
        }
        if (r > 0) {
            frag->usedSize_ = r;
            incoming_fragment(sin, frag, now);
            //  frag is now potentiall gone!
        }
    }

    //  time out old receivers and old packets
    if ((lastCheckTime_ == 0) || (now - lastCheckTime_ >= 1)) {
        check_receivers(now);
        check_packets(now);
        check_senders(now);
    }
}

void Network::check_receivers(double now) {
    auto ptr(receivers_.begin()), end(receivers_.end()), copy(end);
    while (ptr != end) {
        copy = ptr;
        ++ptr;
        if (now - (*copy).second->lastTime_ > packet_timeout) {
            status_->message("Timing out receipt for peer " + ipaddr((*copy).first));
            receivers_.erase(copy);
        }
    }
}

void Network::check_senders(double now) {
    //  time out senders that haven't had any data for X seconds
    auto ptr(outqueue_.begin()), end(outqueue_.end()), copy(end);
    while (ptr != end) {
        copy = ptr;
        ++ptr;
        if (now - (*copy).lastTime_ > packet_timeout) {
            status_->message("Timing out sending for peer " + ipaddr((*copy).addr_));
            outqueue_.erase(copy);
        }
    }
}

void Network::check_packets(double now) {
    //  time out packets that have not received fragments for X seconds
    for (auto ptr(receivers_.begin()), end(receivers_.end()); ptr != end; ++ptr) {
        auto fcs((*ptr).second->fragments_);
        auto x(fcs.begin()), y(fcs.end()), d(y);
        while (x != y) {
            d = x;
            ++x;
            if (now - (*d).lastTime_ > packet_timeout) {
                status_->message("Timeout waiting for fragments from " + ipaddr((*ptr).first));
                fcs.erase(d);
            }
        }
    }
}

void Network::incoming_fragment(sockaddr_in const &from, boost::shared_ptr<fragment> &frag, double atTime) {

    if (locked_) {
        if (from != remoteAddr_) {
            //  ignore packets from non-locked sources
            return;
        }
        lastLockReceiveTime_ = atTime;
    }

    if (frag->usedSize_ - frag->offset_ < 10) {
        //  bah! a packet of bad format. Probably not our protocol.
        status_->message("Bad packet from " + ipaddr(from) + ".");
        return;
    }

    unsigned char const *buf = frag->buf_ + frag->offset_;
    unsigned short seq = buf[0] + (buf[1] << 8);
    unsigned short seg = buf[2] + (buf[3] << 8);
    unsigned short cnt = buf[4] + (buf[5] << 8);
    if (seg >= cnt) {
        status_->message("Remote peer " + ipaddr(from) + " sent fragment index " + hexnum(seg)
            + " out of range " + hexnum(cnt) + ".");
        return;
    }

    unsigned char const *bufEnd = buf + frag->usedSize_ - frag->offset_;
    uint32_t fnv2 = fnv2_hash(frag->buf_ + frag->offset_, frag->usedSize_ - frag->offset_ - 4);
    uint32_t packetChecksum = (uint32_t)bufEnd[-4] + ((uint32_t)bufEnd[-3] << 8) +
        ((uint32_t)bufEnd[-2] << 16) + ((uint32_t)bufEnd[-1] << 24);
    if (fnv2 != packetChecksum) {
        status_->message("Remote peer " + ipaddr(from) + " sent packet with bad checksum.");
        return;
    }
    //  extract the actual data from the fragment
    frag->offset_ += 6;     //  header
    frag->usedSize_ -= 4;   //  checksum

    auto ptr(receivers_.find(from));
    if (ptr == receivers_.end()) {
        //  a new IP sent a seemingly good packet
        status_->message("New remote peer " + ipaddr(from) + ".");
        ptr = receivers_.insert(recv_map::value_type(from,
            boost::shared_ptr<receive_info>(new receive_info(from)))).first;
    }
    (*ptr).second->lastTime_ = atTime;

    if (cnt == 1 && seg == 0) {
        //  packets of a single fragment don't need to go through assembly
        complete_fragment(from, frag);
        return;
    }
    receive_info &ri(*(*ptr).second);
    auto &rfc(ri.fragments_);
    for (auto ptr(rfc.begin()), end(rfc.end());
            ptr != end; ++ptr) {
        if ((*ptr).seq_ ==  seq) {
            auto &pf((*ptr).fragments_);
            if (pf.size() != cnt) {
                status_->message("Remote peer " + ipaddr(from) + " sent bad fragment count.");
                return;
            }
            (*ptr).lastTime_ = atTime;
            pf[seg] = frag;
            for (auto x(pf.begin()), y(pf.end()); x != y; ++x) {
                if (!*x) {
                    //  not yet complete
                    return;
                }
            }
            //  complete
            complete_fragment(from, *ptr);
            return;
        }
    }
    rfc.push_front(fragment_collection(seq, atTime, frag));
    rfc.front().fragments_.resize(cnt);
}

void Network::lock_address(double timeout) {
    lockTimeout_ = timeout;
    if (locked_) {
        return;
    }
    status_->message("Locking connection to peer " + ipaddr(remoteAddr_) + ".");
    locked_ = true;

    //  keep only the data about the locked address
    boost::shared_ptr<receive_info> kept = receivers_[remoteAddr_];
    receivers_.clear();
    receivers_[remoteAddr_] = kept;

    //  keep only packets from the locked address
    auto ptr(inqueue_.begin()), end(inqueue_.end()), copy(end);
    while (ptr != end) {
        copy = ptr;
        ++ptr;
        if ((*copy)->from_ != remoteAddr_) {
            inqueue_.erase(copy);
        }
    }

    //  don't purge packets *to* other addresses!
}

void Network::unlock_address() {
    if (!locked_) {
        return;
    }
    status_->message("Unlocking connection to " + ipaddr(remoteAddr_) + ".");
    locked_ = false;
}

bool Network::is_locked() {
    return locked_;
}

void Network::complete_fragment(sockaddr_in const &from, boost::shared_ptr<fragment> &frag) {
    inqueue_.push_back(boost::shared_ptr<deliver_fragment>(new deliver_fragment(from, *frag)));
}

void Network::complete_fragment(sockaddr_in const &from, fragment_collection const &fc) {
    size_t sz = 0;
    for (auto ptr(fc.fragments_.begin()), end(fc.fragments_.end()); ptr != end; ++ptr) {
        sz += (*ptr)->usedSize_ - (*ptr)->offset_;
    }
    boost::shared_ptr<fragment> frag(new fragment(sz));
    frag->offset_ = 0;
    frag->usedSize_ = sz;
    sz = 0;
    for (auto ptr(fc.fragments_.begin()), end(fc.fragments_.end()); ptr != end; ++ptr) {
        memcpy(frag->buf_ + sz, (*ptr)->buf_ + (*ptr)->offset_, (*ptr)->usedSize_ - (*ptr)->offset_);
        sz += (*ptr)->usedSize_ - (*ptr)->offset_;
    }
    complete_fragment(from, frag);
}

bool Network::receive(size_t &size, void const *&packet) {
    if (inqueue_.empty()) {
        size = 0;
        packet = 0;
        return false;
    }
    curFrag_ = inqueue_.front();
    remoteAddr_ = curFrag_->from_;
    size = curFrag_->usedSize_ - curFrag_->offset_;
    packet = curFrag_->buf_ + curFrag_->offset_;
    inqueue_.pop_front();
    return true;
}

void Network::broadcast(size_t size, void const *packet) {
    iovec iov[1];
    iov[0].iov_base = const_cast<void *>(packet);
    iov[0].iov_len = size;
    vsend(false, 1, iov);
}

void Network::vsend(bool response, size_t count, iovec const *vecs) {
    sockaddr_in dest = remoteAddr_;
    if (!response && !locked_) {
        if (!broadcastOk_) {
            throw std::runtime_error("Cannot broadcast when not locked and not scanning.");
        }
        memset(&dest.sin_addr, 0xff, sizeof(dest.sin_addr));
        dest.sin_port = 0;
    }
    enqueue(dest, count, vecs);
}

static void vec_cpy(unsigned char *dst, size_t cnt, iovec &cur, iovec const *&next) {
    while (cnt > 0) {
        while (cur.iov_len == 0) {
            cur = *next;
            ++next;
        }
        size_t toget = cnt;
        if (toget > cur.iov_len) {
            toget = cur.iov_len;
        }
        if (toget > cnt) {
            toget = cnt;
        }
        memcpy(dst, cur.iov_base, toget);
        cur.iov_base = (char *)cur.iov_base + toget;
        cur.iov_len -= toget;
        cnt -= toget;
        dst += toget;
    }
}

void Network::enqueue(sockaddr_in const &dest, size_t count, iovec const *vecs) {
    auto ptr(outqueue_.begin()), end(outqueue_.end());
    while (ptr != end) {
        if ((*ptr).addr_ == dest) {
            break;
        }
        ++ptr;
    }
    if (ptr == end) {
        outqueue_.push_back(send_info(dest));
        ptr = outqueue_.end();
        --ptr;
    }
    (*ptr).lastTime_ = time_->now();
    size_t size = 0;
    for (size_t iv = 0; iv != count; ++iv) {
        size += vecs[iv].iov_len;
    }
    size_t nfrag = 0;
    if (size > 0) {
        nfrag = (size - 1) / (MAX_FRAGMENT_SIZE - 10) + 1;
    }
    if (nfrag > 65535) {    //  save 0xffff for special signal to use later
        //  with 2048 frag size, that's almost 128 megabytes...
        throw std::runtime_error("Attempt to send too big a packet.");
    }
    iovec avec = { 0, 0 };
    unsigned short seg = 0;
    unsigned short nseg = (unsigned short)nfrag;
    unsigned short seq = (*ptr).nextSeq_;
    (*ptr).nextSeq_++;
    while (size > 0) {
        boost::shared_ptr<fragment> frag(new fragment(MAX_FRAGMENT_SIZE));
        unsigned char *buf = frag->buf_;
        buf[0] = seq & 0xff;
        buf[1] = (seq >> 8) & 0xff;
        buf[2] = seg & 0xff;
        buf[3] = (seg >> 8) & 0xff;
        buf[4] = nseg & 0xff;
        buf[5] = (nseg >> 8) & 0xff;
        size_t tocopy = size;
        if (tocopy > MAX_FRAGMENT_SIZE - 10) {
            tocopy = MAX_FRAGMENT_SIZE - 10;
        }
        vec_cpy(buf + 6, tocopy, avec, vecs);
        size -= tocopy;
        uint32_t cs = fnv2_hash(buf, tocopy + 6);
        buf += tocopy + 6;
        buf[0] = cs & 0xff;
        buf[1] = (cs >> 8) & 0xff;
        buf[2] = (cs >> 16) & 0xff;
        buf[3] = (cs >> 24) & 0xff;
        frag->usedSize_ = tocopy + 10;
        (*ptr).fragments_.push_back(frag);
        seg += 1;
    }
}

void Network::respond(size_t size, void const *packet) {
    iovec iov[1];
    iov[0].iov_base = const_cast<void *>(packet);
    iov[0].iov_len = size;
    enqueue(remoteAddr_, 1, iov);
}


INetwork *listen(ISockets *socks, ITime *itime, IStatus *istatus) {
    return new Network(socks, itime, istatus, false);
}

INetwork *scan(ISockets *socks, ITime *itime, IStatus *istatus) {
    return new Network(socks, itime, istatus, true);
}

