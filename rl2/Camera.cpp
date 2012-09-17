
#include "Camera.h"
#include "Image.h"
#include "Settings.h"

#include <fcntl.h>
#include <unistd.h>

#include <boost/date_time/posix_time/posix_time.hpp>

#include <libv4l2.h>
#include <linux/videodev2.h>


static std::string str_image("image");
static std::string str_fps("fpscap");
static std::string str_fpsstep("fpsstep");
static std::string str_underflows("underflows");

Camera::Camera(std::string const &devname) :
    devname_(devname),
    fd_(::v4l2_open(devname.c_str(), O_RDWR)),
    imageGrabbed_(0),
    lastReturned_(1),
    lastQueued_(1),
    inQueue_(0),
    imageReturned_(Camera::NUM_BUFS - 1),
    fps_(30),
    fpsStep_(30),
    underflows_(0),
    imageProperty_(new PropertyImpl<boost::shared_ptr<Image>>(str_image)),
    fpsProperty_(new PropertyImpl<double>(str_fps)),
    fpsStepProperty_(new PropertyImpl<double>(str_fpsstep)),
    underflowsProperty_(new PropertyImpl<long>(str_underflows)) {
    if (fd_ < 0) {
        throw std::runtime_error("Could not open camera: " + devname);
    }
    configure_dev();
    configure_buffers();
    lastTime_ = boost::get_system_time();
    lastStepTime_ = lastTime_;
    thread_ = boost::shared_ptr<boost::thread>(new boost::thread(&Camera::thread_fn, this));
}

Camera::~Camera() {
    thread_->interrupt();
    thread_->join();
    ::close(fd_);
}

boost::shared_ptr<Module> Camera::open(boost::shared_ptr<Settings> const &set) {
    return boost::shared_ptr<Module>(new Camera(set->get_value("device")->get_string()));
}

std::string const &Camera::name() {
    return devname_;
}

size_t Camera::num_properties() {
    return 4;
}

boost::shared_ptr<Property> Camera::get_property_at(size_t ix) {
    switch (ix) {
        case 0:
            return imageProperty_;
        case 1:
            return fpsProperty_;
        case 2:
            return fpsStepProperty_;
        case 3:
            return underflowsProperty_;
        default:
            throw std::runtime_error("Attempt to get_property_at() beyond range in Camera");
    }
}

void Camera::thread_fn(void *arg) {
    reinterpret_cast<Camera *>(arg)->process();
}

void Camera::step() {
    boost::system_time nuTime = boost::get_system_time();
    boost::posix_time::time_duration delta = nuTime - lastStepTime_;
    lastStepTime_ = nuTime;
    fpsStep_ = fpsStep_ * 0.75 + 0.25 * 1000000.0 / std::max(delta.total_microseconds(), 1L);
    if (imageGrabbed_.nonblocking_available()) {
        imageGrabbed_.acquire();
        imageProperty_->set(forGrabbing_[lastQueued_]);
        lastQueued_ += 1;
        if (lastQueued_ == NUM_BUFS) {
            lastQueued_ = 0;
        }
        fpsProperty_->set(fps_);
        fpsStepProperty_->set(fpsStep_);
        underflowsProperty_->set(underflows_);
        //  release whatever is oldest in the queue
        imageReturned_.release();
    }
}

void Camera::process() {
    lastTime_ = boost::get_system_time();
    sched_param parm = { .sched_priority = 30 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "Camera::process(): pthread_setschedparam(): " << err << std::endl;
    }
    bool inUnderflow = false;
    while (!thread_->interruption_requested()) {
        while (imageReturned_.nonblocking_available()) {
            imageReturned_.acquire();
            queue(forGrabbing_[lastReturned_]);
            lastReturned_ += 1;
            if (lastReturned_ == NUM_BUFS) {
                lastReturned_ = 0;
            }
        }
        if (inQueue_) {
            inUnderflow = false;
            //  wait for fd
            wait();
            boost::system_time nuTime = boost::get_system_time();
            boost::posix_time::time_duration delta = nuTime - lastTime_;
            lastTime_ = nuTime;
            fps_ = fps_ * 0.75 + 0.25 * 1000000.0 / std::max(delta.total_microseconds(), 1L);
            inQueue_ -= 1;
            //  grabbed should be given away
            imageGrabbed_.release();
        }
        else {
            if (!inUnderflow) {
                inUnderflow = true;
                //  oops! I'm underflowing. Gotta wait for the user to return buffers
                std::cerr << "underflow " << devname_ << std::endl;
            }
            ++underflows_;
            boost::this_thread::sleep(boost::posix_time::milliseconds(5));
        }
    }
    std::cerr << "Camera::process() end" << std::endl;
}

void Camera::queue(boost::shared_ptr<Image> const &img) {
    inQueue_ += 1;
}

void Camera::wait() {
    boost::this_thread::yield();
}

void Camera::configure_dev() {
    struct v4l2_capability cap;
    if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_QUERYCAP) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
        int en = errno;
        std::string error = "device does not support mmap() streaming: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = 1280;
    fmt.fmt.pix.height = 720;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    fmt.fmt.pix.bytesperline = 1280*720*3;
    fmt.fmt.pix.sizeimage = 1280*720*3;
    if (v4l2_ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_S_FMT) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
}

void Camera::configure_buffers() {
    memset(&rbufs_, 0, sizeof(rbufs_));
    rbufs_.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rbufs_.memory = V4L2_MEMORY_MMAP;
    rbufs_.count = NUM_BUFS;
    if (v4l2_ioctl(fd, VIDIOC_REQBUFS, &rbufs_) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_REQBUFS) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
    memset(vbufs_, 0, sizeof(vbufs_));
    for (unsigned int i = 0, n = NUM_BUFS; ++i) {
        struct v4l2_buffer vbuf;
        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        vbuf.index = i;
        if (v4l2_ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0) {
            int en = errno;
            std::string error = "ioctl(VIDIOC_QUERYBUF) failed: ";
            error += strerror(en);
            throw std::runtime_error(error);
        }
        .... must re-do the interface for compressed data in Image ....
        forGrabbing_[i]->alloc_compressed(1280*720*4+4096);
        capi->bufs[i].size = vbuf.length;
        capi->bufs[i].ptr = mmap(
                0, 
                vbuf.length, 
                PROT_READ | PROT_WRITE, 
                MAP_SHARED, 
                fd, 
                vbuf.m.offset);
        if (MAP_FAILED == capi->bufs[i].ptr) {
            int en = errno;
            error = "mmap() failed: ";
            error += strerror(en);
            return -1;
        }
        capi->vbufs[i] = vbuf;

    }
}

