
#include "semaphore.h"
#include <assert.h>


semaphore::semaphore(size_t cnt) :
    count_(cnt) {
}

semaphore::~semaphore() {
}

void semaphore::acquire() {
    acquire_n(1);
}

void semaphore::acquire_n(int n) {
    assert(n > 0);
    boost::unique_lock<boost::mutex> lock(guard_);
    while (true) {
        if (count_ >= (size_t)n) {
            count_ -= n;
            return;
        }
        condition_.wait(lock);
    }
}

void semaphore::release() {
    release_n(1);
}

void semaphore::release_n(int n) {
    assert(n > 0);
    boost::unique_lock<boost::mutex> lock(guard_);
    count_ += n;
    condition_.notify_one();
}

size_t semaphore::nonblocking_available() {
    return count_;
}

