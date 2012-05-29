#if !defined(VideoCapture_h)
#define VideoCapture_h

#include "Talker.h"
#include <boost/thread.hpp>

struct capture_info;

class VideoCapture : public Talker {
public:
    VideoCapture(char const *dev);
    ~VideoCapture();

    //  This may call invalidate() and signal listeners
    void step();

    //  Callable from within the invalidate callback
    void *get_jpg() const;
    size_t get_size() const;
    size_t get_padding() const;
    

    int fd_;
    capture_info *info_;
    void *jpg_;
    size_t size_;
};

extern "C" {

int open_dev(char const *devName);
capture_info *make_capture_info();
int config_dev(int fd, capture_info *capi);
int enqueue_all_buffers(int fd, capture_info *ci);
int start_capture(int fd, capture_info *ci);
int capture_one_frame_and_re_enqueue(int fd, capture_info *ci, void *jpg_buf, size_t &ioSize);
int stop_capture(int fd, capture_info *ci);
int close_video(int fd, capture_info *capi);
char const *get_error();

}

#endif
