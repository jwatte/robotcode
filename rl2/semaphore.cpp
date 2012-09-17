
#include "semaphore.h"

semaphore::semaphore(size_t cnt) :
    count_(cnt) {
}

semaphore::~semaphore() {
}

void semaphore::acquire() {
    boost::unique_lock<boost::mutex> lock(guard_);
    while (true) {
        if (count_ > 0) {
            --count_;
            return;
        }
        condition_.wait(lock);
    }
}

void semaphore::release() {
    boost::unique_lock<boost::mutex> lock(guard_);
    ++count_;
    condition_.notify_one();
}

size_t semaphore::nonblocking_available() {
    return count_;
}
