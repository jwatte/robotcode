
#include "inetwork.h"
#include "itime.h"
#include "fakes.h"
#include "util.h"
#include <iostream>
#include <assert.h>
#include <string.h>


void test_packet_receive() {
    Fakenet f;
    Fakestatus s;
    IPacketizer *ip = packetize(&f, &s);
    unsigned char code = 0xff;
    size_t size = (size_t)-1;
    void const *data = 0;
    assert(!ip->receive(code, size, data));
    f.toReceive_.push_back(std::pair<size_t, void const *>(12, "hello, world!"));
    //  ignore unknown/bad data
    assert(!ip->receive(code, size, data));
    assert(!f.toReceive_.size());
    f.toReceive_.push_back(std::pair<size_t, void const *>(15, "\001\0120123456789\x02\x80\x0"));
    assert(ip->receive(code, size, data));
    assert(code == 1);
    assert(size == 10);
    assert(!strncmp((char const *)data, "0123456789", 10));
    assert(ip->receive(code, size, data));
    assert(code == 2);
    assert(size == 0);
    assert(!ip->receive(code, size, data));
}

void test_packet_send() {
    Fakenet f;
    Fakestatus s;
    IPacketizer *ip = packetize(&f, &s);
    ip->send(1, 10, "0123456789");
    ip->send(2, 0, "");
    assert(f.wereSent_.empty());
    ip->step();
    assert(f.wereSent_.size() == 1);
    assert(f.wereSent_.front().second.size() == 14);
    char buf[14] = "\001\0120123456789\002";
    //  checksum calculated for buf
    assert(!memcmp(&f.wereSent_.front().second[0], buf, 14));
}

void test_network_snark() {
    Faketime t;
    Fakestatus s;
    Fakesockets ss;
    INetwork *n1 = listen(&ss, &t, &s);
    n1->step();
    assert(!ss.toReceive_.size());
    assert(!ss.wasSent_.size());
    size_t rsz = 0;
    void const *pack = 0;
    assert(!n1->receive(rsz, pack));
    Fakesockets::rec r;
    char str[17] = "\0\0\0\0\01\0x123456";
    char const *end = str + 17;
    str[13] = 0x68;
    str[14] = 0xc6;
    str[15] = 0x8a;
    str[16] = 0xef;
    r.data.insert(r.data.end(), (char const *)str, end);
    ss.toReceive_.push_back(r);
    n1->step();
    assert(n1->receive(rsz, pack));
    assert(rsz == 7);
    assert(!memcmp("x123456", pack, 7));
    assert(!n1->receive(rsz, pack));
    char buf[5000];
    for (int i = 0; i < 5000; i += 2) {
        buf[i] = i & 0xff;
        buf[i+1] = (i >> 8) & 0xff;
    }
    n1->respond(5000, buf);
    assert(!ss.wasSent_.size());
    n1->step();
    assert(ss.wasSent_.size() == 3);
    assert(!n1->receive(rsz, pack));
    std::list<Fakesockets::rec> x;
    x.swap(ss.wasSent_);
    ss.toReceive_.push_back(x.front());
    x.pop_front();
    n1->step();
    assert(!n1->receive(rsz, pack));
    n1->step();
    assert(!n1->receive(rsz, pack));
    ss.toReceive_.push_back(x.back());
    x.pop_back();
    n1->step();
    assert(!n1->receive(rsz, pack));
    ss.toReceive_.push_back(x.back());
    x.pop_back();
    n1->step();
    assert(n1->receive(rsz, pack));
    assert(rsz == 5000);
    assert(!memcmp(buf, pack, 5000));
}

void test_hash() {
    uint32_t h1 = fnv2_hash("1234", 4);
    uint32_t h2 = fnv2_hash("123", 3);
    uint32_t h3 = fnv2_hash("321", 3);
    assert(h1 != h2);
    assert(h1 != h3);
    assert(h2 != h3);
}

int main() {
    test_hash();
    test_packet_receive();
    test_packet_send();
    test_network_snark();
    return 0;
}

