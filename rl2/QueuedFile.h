#if !defined(rl2_QueuedFile_h)
#define rl2_Queued_file_h

#include "semaphore.h"
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <string>

struct vlist {
    void const *data;
    size_t size;
};
class QueuedFile : public boost::enable_shared_from_this<QueuedFile> {
public:
    static boost::shared_ptr<QueuedFile> create(std::string const &name);
    void write(void const *data, size_t size);
    void writev(vlist const *ptr, size_t n);
    virtual ~QueuedFile();
private:
    QueuedFile(std::string const &name);
    friend class QueuedFileManager;
    enum {
        //  a meg of buffering -- should be sufficient for MJPEG images, too
        BUF_SIZE = 1024*1024
    };
    char buf_[BUF_SIZE];
    size_t bufHead_;
    size_t bufTail_;
    int fd_;
    semaphore toWrite_;
    semaphore toRead_;
    std::string name_;
    void get_tail(size_t n, void const *&oa, size_t &oasz, void const *&ob, size_t &obsz);
    void get_head(size_t n, void *&oa, size_t &oasz, void *&ob, size_t &obsz);
};

#endif  //  rl2_Queued_file_h

