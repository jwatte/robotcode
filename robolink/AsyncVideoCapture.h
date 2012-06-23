#if !defined(AsyncVideoCapture_h)
#define AsyncVideoCapture_h

#include "Talker.h"
#include "Signal.h"
#include "VideoCapture.h"

class VideoFrame
{
public:
    enum {
        IndexLeft = 0,
        IndexRight = 1,
        MaxIndices = 2,
        PADDING = 4096
    };
    VideoFrame();
    ~VideoFrame();

    double captureTime();
    void const *data(size_t &oSize, unsigned int index);

    void clear();
    void setData(unsigned int index, void const *data, size_t size);
    void setCaptureTime(double ct);
protected:
    double captureTime_;
    void *data_[MaxIndices];
    size_t size_[MaxIndices];
};

class AsyncVideoCapture
{
public:
    AsyncVideoCapture(std::string const &left, std::string const &right);
    ~AsyncVideoCapture();
    bool open();

    bool gotFrame();
    void waitFrame();
    VideoFrame *next();
    VideoFrame *current();
private:
    VideoFrame frame_[2];
    std::string leftName_;
    std::string rightName_;
    VideoCapture *left_;
    VideoCapture *right_;
    volatile bool running_;
    volatile int lastNum_;
    volatile int frameNum_;
    boost::thread *captureThread_;
    Signal frameSignal_;
    Signal captureSignal_;

    void capture_func();
};

#endif  //  AsyncVideoCapture_h
