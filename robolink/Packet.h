#if !defined(Packet_h)
#define Packet_h

#include <stdlib.h>
#include <boost/thread.hpp>

/* This is a safe, lock-free container, as long 
   as only one thread will be calling enqueue(), 
   and only one (other) thread will be calling dequeue().
 */
template<typename T, int N>
class Pipe {
public:
  Pipe() : head_(0), tail_(0) {
    memset(pipe_, 0, sizeof(pipe_));
  }
  /* If empty, return NULL, else remove and return the next element in the queue */
  T *dequeue() {
    if (head_ - tail_ > 0) {
      T *ret = pipe_[head_ & (N-1)];
      ++tail_;
      return ret;
    }
    return 0;
  }
  /* If full, return false, else accept the element into the queue */
  bool enqueue(T *t) {
    if (head_ - tail_ < N) {
      pipe_[head_] = t;
      ++head_;
      return true;
    }
    return false;
  }
private:
  T *pipe_[N];
  int head_;
  int tail_;
};

class Packet {
public:
  Packet();
  static Packet *create();
  void destroy();

  unsigned char *buffer() { return data_; }
  unsigned int size() const { return size_; }
  unsigned int max_size() const { return sizeof(data_); }
  void set(void const *src, unsigned int cnt);
  void add(void const *src, unsigned int cnt);

private:
  ~Packet();
  unsigned char data_[32];
  unsigned int size_;
  static Pipe<Packet, 32> cache_;
  static boost::mutex lock_;
};

class IPacketDestination {
public:
  virtual void on_packet(Packet *p) = 0;
};



#endif  //  Packet_h
