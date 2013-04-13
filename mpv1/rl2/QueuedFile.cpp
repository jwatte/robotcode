
#include "QueuedFile.h"
#include "Services.h"
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <boost/lexical_cast.hpp>
#include <fcntl.h>



class QueuedFileManager {
public:
    QueuedFileManager();
    ~QueuedFileManager();
    void enqueue(boost::shared_ptr<QueuedFile> const &file);
private:
    boost::mutex guard_;
    boost::condition_variable condition_;
    std::set<boost::shared_ptr<QueuedFile>> queue_;
    boost::shared_ptr<boost::thread> thread_;

    void thread_fn();
};

QueuedFileManager::QueuedFileManager() {
    thread_ = boost::shared_ptr<boost::thread>(new boost::thread(
        boost::bind(&QueuedFileManager::thread_fn, this)));
}

QueuedFileManager::~QueuedFileManager() {
    {
        boost::unique_lock<boost::mutex> lock(guard_);
        queue_.clear();
        thread_->interrupt();
        condition_.notify_one();
    }
    thread_->join();
}

void QueuedFileManager::enqueue(boost::shared_ptr<QueuedFile> const &file) {
    boost::unique_lock<boost::mutex> lock(guard_);
    queue_.insert(file);
    condition_.notify_one();
}

void QueuedFileManager::thread_fn() {
    while (!thread_->interruption_requested()) {
        boost::shared_ptr<QueuedFile> file;
        {
            boost::unique_lock<boost::mutex> lock(guard_);
            if (queue_.empty()) {
                condition_.wait(lock);
            }
            if (!queue_.empty()) {
                file = *queue_.begin();
                queue_.erase(queue_.begin());
            }
        }
        if (!!file) {
            size_t sz = file->toRead_.nonblocking_available();
            if (sz > 0) {
                file->toRead_.acquire_n(sz);
                void const *a = 0;
                void const *b = 0;
                size_t sa = 0;
                size_t sb = 0;
                file->get_tail(sz, a, sa, b, sb);
                if ((sa > 0 && gSvc->write(file->fd_, a, sa) < 0) ||
                    (sb > 0 && gSvc->write(file->fd_, b, sb) < 0)) {
                    std::cerr << "file write failed: " << file->name_ << 
                        " (" << sa + sb << " bytes)" << std::endl;
                }
                else {
                    gSvc->fsync(file->fd_);
                }
                file->toWrite_.release_n(sa + sb);
            }
        }
    }
}

static QueuedFileManager g_mgr;



boost::shared_ptr<QueuedFile> QueuedFile::create(std::string const &name) {
    return boost::shared_ptr<QueuedFile>(new QueuedFile(name));
}

void QueuedFile::write(void const *data, size_t size) {
    vlist vl;
    vl.data = data;
    vl.size = size;
    writev(&vl, 1);
}

static void fill(void *d, size_t dz, vlist const *&cur, size_t &used) {
    size_t gone = 0;
    while (gone < dz) {
        size_t tocopy = cur->size - used;
        if (tocopy > dz - gone) {
            tocopy = dz - gone;
        }
        memcpy((char *)d + gone, (char *)cur->data + used, tocopy);
        used += tocopy;
        gone += tocopy;
        if (used == cur->size) {
            ++cur;
            used = 0;
        }
    }
}


void QueuedFile::writev(vlist const *ptr, size_t n) {
    size_t size = 0;
    for (size_t i = 0; i != n; ++i) {
        size += ptr[i].size;
    }
    if (size == 0) {
        return;
    }
    vlist const *cur = ptr;
    //  "used" keeps how much is used from the current vlist item
    size_t used = 0;
    while (size > 0) {
        // get N from cur, and increment ptr if not enough
        if (size >= BUF_SIZE) {
            toWrite_.acquire_n(BUF_SIZE);
            //  At this point, I know the thread is not referencing this file!
            //  This is because I hold a reference to all bytes in the buffer.
            fill(buf_, BUF_SIZE, cur, used);
            bufHead_ = bufTail_ = 0;
            size -= BUF_SIZE;
            toRead_.release_n(BUF_SIZE);
        }
        else {
            toWrite_.acquire_n(size);
            void *a = 0, *b = 0;
            size_t sa = 0, sb = 0;
            get_head(size, a, sa, b, sb);
            if (sa) {
                fill(a, sa, cur, used);
            }
            if (sb) {
                fill(b, sb, cur, used);
            }
            size -= (sa + sb);
            toRead_.release_n(sa + sb);
        }
        g_mgr.enqueue(shared_from_this());
    }
}

QueuedFile::~QueuedFile() {
    gSvc->close(fd_);
}

QueuedFile::QueuedFile(std::string const &name) :
    bufHead_(0),
    bufTail_(0),
    fd_(0),
    toWrite_(BUF_SIZE),
    toRead_(0),
    name_(name) {
    fd_ = gSvc->open(name.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0664);
    if (fd_ < 0) {
        throw std::runtime_error("Could not create file: " + name);
    }
}

void QueuedFile::get_tail(size_t n, void const *&oa, size_t &oasz, void const *&ob, size_t &obsz) {
    assert(n <= BUF_SIZE);
    obsz = 0;
    ob = 0;
    oa = &buf_[bufTail_ & (BUF_SIZE - 1)];
    oasz = n;
    char const *end = (char const *)oa + oasz;
    if (end > &buf_[BUF_SIZE]) {
        obsz = end - &buf_[BUF_SIZE];
        oasz -= obsz;
        ob = buf_;
    }
    bufTail_ += n;
}

void QueuedFile::get_head(size_t n, void *&oa, size_t &oasz, void *&ob, size_t &obsz) {
    assert(n <= BUF_SIZE);
    obsz = 0;
    ob = 0;
    oa = &buf_[bufHead_ & (BUF_SIZE - 1)];
    oasz = n;
    char *end = (char *)oa + oasz;
    if (end > &buf_[BUF_SIZE]) {
        obsz = end - &buf_[BUF_SIZE];
        oasz -= obsz;
        ob = buf_;
    }
    bufHead_ += n;
}

