#if !defined(rl2_semaphore_h)
#define rl2_semaphore_h

#include <boost/noncopyable.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/condition_variable.hpp>

class semaphore : public boost::noncopyable {
public:
    semaphore(size_t cnt);
    ~semaphore();

    void acquire();
    void acquire_n(int n);
    void release();
    void release_n(int n);
    //  nonblocking_available is only safe if there is only 
    //  ever one thread that will call acquire() on the 
    //  semaphore.
    size_t nonblocking_available();

private:
    volatile size_t count_;
    boost::mutex guard_;
    boost::condition_variable condition_;
    
};

#endif  //  rl2_semaphore_h
