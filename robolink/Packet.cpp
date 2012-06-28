
#include <string.h>
#include <stdexcept>
#include <boost/thread.hpp>

#include "Packet.h"



Pipe<Packet, 32> Packet::cache_;
boost::mutex Packet::lock_;

Packet::Packet() :
  size_(0)
{
  memset(data_, 0, sizeof(data_));
}

Packet::~Packet()
{
}

Packet *Packet::create()
{
    boost::unique_lock<boost::mutex> lock(lock_);
    Packet *ret = cache_.dequeue();
    if (!ret) {
        ret = new Packet();
    }
    return ret;
}

void Packet::destroy()
{
    boost::unique_lock<boost::mutex> lock(lock_);
    if (!cache_.enqueue(this)) {
        delete this;
    }
}

void Packet::set(void const *src, unsigned int cnt)
{
  if (cnt > sizeof(data_)) {
    throw std::runtime_error("Too large data in Packet::set()");
  }
  memcpy(data_, src, cnt);
  size_ = cnt;
}

void Packet::add(void const *src, unsigned int cnt)
{
  if (size_ + cnt > sizeof(data_)) {
    throw std::runtime_error("Too large data in Packet::add()");
  }
  memcpy(&data_[size_], src, cnt);
  size_ += cnt;
}



