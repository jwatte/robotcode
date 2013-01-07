
#include "fakes.h"
#include <iostream>


Fakenet::Fakenet() {
    stepCnt_ = 0;
    wasLocked_ = false;
    wasUnlocked_ = false;
    locked_ = false;
    timeout_ = 0;
}

void Fakenet::step() {
    ++stepCnt_;
}

bool Fakenet::receive(size_t &size, void const *&packet) {
    if (!toReceive_.size()) {
        return false;
    }
    size = toReceive_.front().first;
    packet = toReceive_.front().second;
    toReceive_.pop_front();
    return true;
}

void Fakenet::broadcast(size_t size, void const *packet) {
    iovec iov[1];
    iov[0].iov_base = const_cast<void *>(packet);
    iov[0].iov_len = size;
    vsend(false, 1, iov);
}

void Fakenet::respond(size_t size, void const *packet) {
    iovec iov[1];
    iov[0].iov_base = const_cast<void *>(packet);
    iov[0].iov_len = size;
    vsend(true, 1, iov);
}

void Fakenet::vsend(bool response, size_t cnt, iovec const *vecs) {
    std::vector<char> sent;
    while (cnt > 0) {
        sent.insert(sent.end(), (char const *)vecs->iov_base, (char const *)vecs->iov_base + vecs->iov_len);
        ++vecs;
        --cnt;
    }
    wereSent_.push_back(std::pair<bool, std::vector<char>>(response, sent));
}

void Fakenet::lock_address(double timeout) {
    locked_ = true;
    wasLocked_ = true;
    timeout_ = timeout;
}

void Fakenet::unlock_address() {
    wasUnlocked_ = true;
    locked_ = false;
}

bool Fakenet::is_locked() {
    return locked_;
}



void Fakestatus::message(std::string const &str) {
    messages_.push_back(std::pair<bool, std::string>(false, str));
}

void Fakestatus::error(std::string const &str) {
    messages_.push_back(std::pair<bool, std::string>(true, str));
}

size_t Fakestatus::n_messages() {
    return 0;
}

bool Fakestatus::get_message(bool &isError, std::string &oMessage) {
    return false;
}


int Fakesockets::recvfrom(void *buf, size_t sz, sockaddr_in &addr) {
    if (toReceive_.size() > 0) {
        rec r = toReceive_.front();
        toReceive_.pop_front();
        if (r.ret < 0) {
            return r.ret;
        }
        if (r.data.size() > 0) {
            memcpy(buf, &r.data[0], std::min(sz, r.data.size()));
        }
        r.addr = addr;
        return std::min(sz, r.data.size());
    }
    return -1;
}

int Fakesockets::sendto(void const *buf, size_t sz, sockaddr_in const &addr) {
    wasSent_.push_back(rec());
    wasSent_.back().ret = sz;
    wasSent_.back().addr = addr;
    wasSent_.back().data.insert(wasSent_.back().data.end(), (char const *)buf, (char const *)buf + sz);
    return sz;
}



Faketime::Faketime() {
    timeSlept_ = 0;
    time_ = 1;
    dTime_ = 0.008;
}

double Faketime::now() {
    time_ += dTime_;
    return time_;
}

void Faketime::sleep(double dt) {
    timeSlept_ += dt;
    time_ += dt;
}



