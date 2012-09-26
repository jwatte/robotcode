
#include "semaphore.h"

semaphore::semaphore(size_t cnt) :
    count_(cnt) {
}

semaphore::~semaphore() {
}

void semaphore::acquire() {
    acquire_n(1);
}

void semaphore::acquire_n(int n) {
    boost::unique_lock<boost::mutex> lock(guard_);
    while (true) {
        if (count_ >= n) {
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
    boost::unique_lock<boost::mutex> lock(guard_);
    count_ += n;
    condition_.notify_one();
}

size_t semaphore::nonblocking_available() {
    return count_;
}

