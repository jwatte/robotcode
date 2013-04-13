#include "Signal.h"

Signal::Signal()
{
}

void Signal::wait()
{
    boost::unique_lock<boost::mutex> lock(lock_);
    while (!ready_)
    {
        cond_.wait(lock);
    }
    ready_ = false;
}

void Signal::set()
{
    boost::unique_lock<boost::mutex> lock(lock_);
    ready_ = true;
    cond_.notify_one();
}


Semaphore::Semaphore(int cnt) :
    cnt_(cnt),
    cancel_(false)
{
}

void Semaphore::wait(int cnt)
{
    boost::unique_lock<boost::mutex> lock(lock_);
    while (true) {
        if (cnt_ >= cnt) {
            cnt_ -= cnt;
            return;
        }
        cond_.wait(lock);
        if (cancel_) {
            cond_.notify_one();
            throw std::runtime_error("Semaphore Cancelled");
        }
    }
}

size_t Semaphore::wait_max()
{
    boost::unique_lock<boost::mutex> lock(lock_);
    while (true) {
        if (cnt_ > 0) {
            size_t s = cnt_;
            cnt_ = 0;
            return s;
        }
        cond_.wait(lock);
        if (cancel_) {
            cond_.notify_one();
            throw std::runtime_error("Semaphore Cancelled");
        }
    }
}

void Semaphore::set(int cnt)
{
    boost::unique_lock<boost::mutex> lock(lock_);
    cnt_ += cnt;
    cond_.notify_one();
}

void Semaphore::cancel()
{
    boost::unique_lock<boost::mutex> lock(lock_);
    cancel_ = true;
    cond_.notify_one();
}

