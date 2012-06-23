#if !defined(Signal_h)
#define Signal_h

#include <boost/thread.hpp>

class Signal
{
public:
    Signal();
    void wait();
    void set();
private:
    volatile bool ready_;
    boost::mutex lock_;
    boost::condition_variable cond_;
};

class Semaphore
{
public:
    Semaphore(int cnt);
    void wait(int cnt);
    size_t wait_max();
    void set(int cnt);
    void cancel();
private:
    volatile int cnt_;
    volatile bool cancel_;
    boost::mutex lock_;
    boost::condition_variable cond_;
};

#endif
