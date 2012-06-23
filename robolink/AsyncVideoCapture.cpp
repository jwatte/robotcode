
#include "AsyncVideoCapture.h"
#include "Time.h"

#include <boost/bind.hpp>


AsyncVideoCapture::AsyncVideoCapture(std::string const &left, std::string const &right) :
    leftName_(left),
    rightName_(right),
    left_(0),
    right_(0),
    running_(false),
    lastNum_(0),
    frameNum_(0),
    captureThread_(0)
{
}

AsyncVideoCapture::~AsyncVideoCapture()
{
    if (captureThread_)
    {
        running_ = false;
        captureThread_->join();
        delete captureThread_;
        delete left_;
        left_ = 0;
        delete right_;
        right_ = 0;
        captureThread_ = 0;
    }
}

bool AsyncVideoCapture::open()
{
    if (captureThread_)
    {
        return false;
    }
    left_ = new VideoCapture(leftName_);
    right_ = new VideoCapture(rightName_);
    running_ = true;
    captureSignal_.set();
    captureThread_ = new boost::thread(boost::bind(&AsyncVideoCapture::capture_func, this));
    return true;
}

bool AsyncVideoCapture::gotFrame()
{
   return (frameNum_ - lastNum_) > 0;
}

void AsyncVideoCapture::waitFrame()
{
    while (!gotFrame())
    {
        frameSignal_.wait();
    }
}

VideoFrame *AsyncVideoCapture::current()
{
    return &frame_[lastNum_ & 1];
}

VideoFrame *AsyncVideoCapture::next()
{
    if (gotFrame()) {
        ++lastNum_;
        captureSignal_.set();
        return &frame_[lastNum_ & 1];
    }
    return 0;
}

void AsyncVideoCapture::capture_func()
{
    while (running_)
    {
        captureSignal_.wait();
        if (frameNum_ == lastNum_)
        {
            //  capture a frame
            left_->step();
            double t = now();
            right_->step();
            int fn = (frameNum_ + 1) & 1;
            frame_[fn].setCaptureTime(t);
            frame_[fn].setData(VideoFrame::IndexLeft, 
                left_->get_jpg(), left_->get_size());
            frame_[fn].setData(VideoFrame::IndexRight,
                right_->get_jpg(), right_->get_size());
            ++frameNum_;
        }
    }
}



VideoFrame::VideoFrame()
{
    data_[0] = malloc(100000);
    data_[1] = malloc(100000);
}

VideoFrame::~VideoFrame()
{
    free(data_[0]);
    free(data_[1]);
}

double VideoFrame::captureTime()
{
    return captureTime_;
}

void const *VideoFrame::data(size_t &oSize, unsigned int index)
{
    oSize = size_[index];
    return (char *)data_[index] + PADDING;
}

void VideoFrame::clear()
{
    size_[0] = size_[1] = 0;
}

void VideoFrame::setData(unsigned int index, void const *data, size_t size)
{
    void *ra = realloc(data_[index], size + PADDING);
    memcpy((char *)ra + PADDING, data, size);
    data_[index] = ra;
    size_[index] = size;
}

void VideoFrame::setCaptureTime(double ct)
{
    captureTime_ = ct;
}

