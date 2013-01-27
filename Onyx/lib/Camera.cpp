
#include "Camera.h"
#include "Image.h"
#include "Settings.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include <boost/date_time/posix_time/posix_time.hpp>

static std::string str_image("image");
static std::string str_fps("fpscap");
static std::string str_fpsstep("fpsstep");
static std::string str_underflows("underflows");

Camera::Camera(std::string const &devname, unsigned int capWidth, unsigned int capHeight) :
    devname_(devname),
    fd_(::v4l2_open(devname.c_str(), O_RDWR)),
    imageGrabbed_(0),
    capWidth_(capWidth),
    capHeight_(capHeight),
    inQueue_(0),
    //  number of returned images must be 1 less than total number,
    //  to allow one to be in use by the main/displaying thread
    imageReturned_(Camera::NUM_BUFS - 1),
    nextImgToDisplay_(0), // nextImgToUse_ -> nextImgToDisplay_
    nextImgToUse_(0),
    fps_(30),
    fpsStep_(30),
    underflows_(0),
    nbad_(0),
    imageProperty_(new PropertyImpl<boost::shared_ptr<Image>>(str_image)),
    fpsProperty_(new PropertyImpl<double>(str_fps)),
    fpsStepProperty_(new PropertyImpl<double>(str_fpsstep)),
    underflowsProperty_(new PropertyImpl<long>(str_underflows)) {
    if (fd_ < 0) {
        throw std::runtime_error("Could not open camera: " + devname);
    }
    for (size_t i = 0; i != NUM_BUFS; ++i) {
        forGrabbing_[i] = boost::shared_ptr<Image>(new Image());
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
    int capWidth = 1920;
    int capHeight = 1080;
    auto v = set->get_value("width");
    if (!!v) {
        capWidth = v->get_long();
    }
    v = set->get_value("height");
    if (!!v) {
        capHeight = v->get_long();
    }
    return boost::shared_ptr<Module>(new Camera(set->get_value("device")->get_string(),
        capWidth, capHeight));
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
    Camera *c = reinterpret_cast<Camera *>(arg);
    try {
        c->process();
    }
    catch (std::runtime_error const &x) {
        std::cerr << "Camera " << c->devname_ << " thread exception: " << x.what() << std::endl;
        throw;
    }
    catch (...) {
        std::cerr << "Camera " << c->devname_ << " unknown exception." << std::endl;
        throw;
    }
}

void Camera::step() {
    boost::system_time nuTime = boost::get_system_time();
    boost::posix_time::time_duration delta = nuTime - lastStepTime_;
    lastStepTime_ = nuTime;
    fpsStep_ = fpsStep_ * 0.75 + 0.25 * 1000000.0 / std::max((long long)delta.total_microseconds(), 1LL);
    if (imageGrabbed_.nonblocking_available()) {
        imageGrabbed_.acquire();
        imageProperty_->set(forGrabbing_[nextImgToDisplay_]);
        nextImgToDisplay_ += 1;
        if (nextImgToDisplay_ == NUM_BUFS) {
            nextImgToDisplay_ = 0;
        }
        fpsProperty_->set(fps_);
        fpsStepProperty_->set(fpsStep_);
        underflowsProperty_->set(underflows_);
        //  release whatever is oldest in the queue -- nextImgToUse_
        imageReturned_.release();
    }
}

void Camera::start_capture() {
    int strm = 1;
    if (v4l2_ioctl(fd_, VIDIOC_STREAMON, &strm) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_STREAMON) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
}

void Camera::stop_capture() {
    int strm = 1;
    if (v4l2_ioctl(fd_, VIDIOC_STREAMOFF, &strm) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_STREAMOFF) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
}

void Camera::poll_and_queue() {
    while (imageReturned_.nonblocking_available()) {
        imageReturned_.acquire();
        queue();
    }
}

void Camera::process() {
    lastTime_ = boost::get_system_time();
    sched_param parm = { .sched_priority = 20 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "Camera::process(): pthread_setschedparam(): " << err << std::endl;
    }

    poll_and_queue();
    start_capture();

    bool inUnderflow = false;
    while (!thread_->interruption_requested()) {
        poll_and_queue();
        if (inQueue_) {
            inUnderflow = false;
            //  wait for fd
            wait();
            boost::system_time nuTime = boost::get_system_time();
            boost::posix_time::time_duration delta = nuTime - lastTime_;
            lastTime_ = nuTime;
            double instant = 1000000.0 / std::max((long long)delta.total_microseconds(), 1LL);
            //  quick hack to make the FPS update smoother at high rates and 
            //  less laggy at lower rates. I don't feel like working out the 
            //  exp math to make it constant sleew rate over time.
            double weightA = 0.15 + 0.5 / instant;
            if (weightA > 1) {
                weightA = 1;
            }
            fps_ = (1 - weightA) * fps_ + weightA * instant;
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
    stop_capture();
    std::cerr << "Camera::process() end" << std::endl;
}

void Camera::queue() {
    size_t ix = NUM_BUFS;
    for (size_t i = 0; i != NUM_BUFS; ++i) {
        if (!bufs_[nextVbufToUse_].queued) {
            bufs_[nextVbufToUse_].queued = true;
            ix = nextVbufToUse_;
            break;
        }
        nextVbufToUse_ += 1;
        if (nextVbufToUse_ == NUM_BUFS) {
            nextVbufToUse_ = 0;
        }
    }
    assert(ix != NUM_BUFS);
    if (v4l2_ioctl(fd_, VIDIOC_QBUF, &vbufs_[ix]) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_QBUF) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
    inQueue_ += 1;
}

void Camera::wait() {
    v4l2_buffer vbuf;
    memset(&vbuf, 0, sizeof(vbuf));
    vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    vbuf.memory = V4L2_MEMORY_MMAP;
    if (v4l2_ioctl(fd_, VIDIOC_DQBUF, &vbuf) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_DQBUF) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
    bufs_[vbuf.index].queued = false;

    /* shuffle the data over to the output buffer here */
    unsigned int osz = vbuf.bytesused;
    void *iptr = bufs_[vbuf.index].ptr;
    void *optr = forGrabbing_[nextImgToUse_]->alloc_compressed(osz);
    memcpy(optr, iptr, osz);
    try {
        forGrabbing_[nextImgToUse_]->complete_compressed(osz);
        nextImgToUse_ += 1;
        if (nextImgToUse_ == NUM_BUFS) {
            nextImgToUse_ = 0;
        }
        nbad_ = 0;
    }
    catch (std::runtime_error const &re) {
        std::cerr << "Camera got error: " << re.what() << std::endl;
        ++nbad_;
        if (nbad_ >= 10) {
            //rethrow
            throw;
        }
    }
}

void Camera::configure_dev() {
    struct v4l2_capability cap;
    if (v4l2_ioctl(fd_, VIDIOC_QUERYCAP, &cap) < 0) {
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
    /* pring frame rate information
    v4l2_streamparm sprm;
    memset(&sprm, 0, sizeof(sprm));
    sprm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(fd_, VIDIOC_G_PARM, &sprm) >= 0) {
        std::cerr << "  capability  =" << sprm.parm.capture.capability << std::endl;
        std::cerr << "  capturemode =" << sprm.parm.capture.capturemode << std::endl;
        std::cerr << "  timeperframe=" << sprm.parm.capture.timeperframe.numerator
            << "/" << sprm.parm.capture.timeperframe.denominator << std::endl;
        std::cerr << "  extendedmode=" << sprm.parm.capture.extendedmode << std::endl;
        std::cerr << "  readbuffers =" << sprm.parm.capture.readbuffers << std::endl;
    }
    */
    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (v4l2_ioctl(fd_, VIDIOC_G_FMT, &fmt) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_G_FMT) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = capWidth_;
    fmt.fmt.pix.height = capHeight_;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    fmt.fmt.pix.bytesperline = capWidth_ * capHeight_ * 2;
    fmt.fmt.pix.sizeimage = fmt.fmt.pix.bytesperline;
    if (v4l2_ioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
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
    if (v4l2_ioctl(fd_, VIDIOC_REQBUFS, &rbufs_) < 0) {
        int en = errno;
        std::string error = "ioctl(VIDIOC_REQBUFS) failed: ";
        error += strerror(en);
        throw std::runtime_error(error);
    }
    memset(vbufs_, 0, sizeof(vbufs_));
    for (unsigned int i = 0, n = NUM_BUFS; i != n; ++i) {
        struct v4l2_buffer vbuf;
        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        vbuf.index = i;
        if (v4l2_ioctl(fd_, VIDIOC_QUERYBUF, &vbuf) < 0) {
            int en = errno;
            std::string error = "ioctl(VIDIOC_QUERYBUF) failed: ";
            error += strerror(en);
            throw std::runtime_error(error);
        }
        bufs_[i].size = vbuf.length;
        bufs_[i].ptr = mmap(
                0, 
                vbuf.length, 
                PROT_READ | PROT_WRITE, 
                MAP_SHARED, 
                fd_, 
                vbuf.m.offset);
        if ((void *)MAP_FAILED == bufs_[i].ptr) {
            int en = errno;
            std::string error = "mmap() failed: ";
            error += strerror(en);
            throw std::runtime_error(error);
        }
        vbufs_[i] = vbuf;
    }
}

