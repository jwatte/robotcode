
#include "VideoCapture.h"

#include <boost/bind.hpp>

#include <iostream>
#include <string>

#include <libv4l2.h>
#include <linux/videodev2.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>


#define PADDING 4096
#define JPG_SIZE (4*1920*1080 + 2*PADDING)  //  support uncompressed 1920x1080

VideoCapture::VideoCapture(std::string const &dev)
{
    fd_ = -1;
    info_ = 0;
    jpg_ = malloc(JPG_SIZE);
    size_ = JPG_SIZE;
    fd_ = open_dev(dev.c_str());
    if (fd_ > -1) {
        info_ = make_capture_info();
        config_dev(fd_, info_);
        if (enqueue_all_buffers(fd_, info_) < 0) {
            fprintf(stderr, "%s: enqueue error: %s\n", dev.c_str(), get_error());
            return;
        }
        if (start_capture(fd_, info_) < 0) {
            fprintf(stderr, "%s: start error: %s\n", dev.c_str(), get_error());
            return;
        }
    }
    else {
        fprintf(stderr, "%s: error: %s\n", dev.c_str(), get_error());
    }
}

VideoCapture::~VideoCapture()
{
    stop_capture(fd_, info_);
    close_video(fd_, info_);
    free(jpg_);
}

bool VideoCapture::step()
{
    size_ = JPG_SIZE - PADDING;
    if (capture_one_frame_and_re_enqueue(fd_, info_, (char *)jpg_ + PADDING, size_) >= 0)
    {
        invalidate();
        return true;
    }
    return false;
}

void *VideoCapture::get_jpg() const
{
    return (char *)jpg_ + PADDING;
}

size_t VideoCapture::get_size() const
{
    return size_;
}

size_t VideoCapture::get_padding() const
{
    return PADDING;
}



extern "C" {

    //  As long as the frame rate is capped at 7.5 Hz, I only 
    //  need a single buffer.
    unsigned int N_V_BUFS = 1;

    static std::string error;

    char const *get_error()
    {
        return error.c_str();
    }

    int open_dev(char const *devName)
    {
        std::string str(devName);
        int fd = v4l2_open(str.c_str(), O_RDWR);
        if (fd < 0) {
            int en = errno;
            error = "open(" + str + ") failed: ";
            error += strerror(en);
            return -1;
        }
        struct v4l2_capability cap;
        if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
            int en = errno;
            error = "ioctl(VIDIOC_QUERYCAP) failed: ";
            error += strerror(en);
            close(fd);
            return -1;
        }
        if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
            int en = errno;
            error = "device does not support mmap() streaming: ";
            error += strerror(en);
            close(fd);
            return -1;
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
            error = "ioctl(VIDIOC_S_FMT) failed: ";
            error += strerror(en);
            close(fd);
            return -1;
        }
        return fd;
    }

    struct capture_buf {
        void *ptr;
        size_t size;
    };

    struct capture_info {
        v4l2_requestbuffers rbuf;
        v4l2_buffer *vbufs;
        capture_buf *bufs;
    };

    int config_dev(int fd, capture_info *capi)
    {
        memset(capi, 0, sizeof(*capi));
        capi->rbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        capi->rbuf.memory = V4L2_MEMORY_MMAP;
        capi->rbuf.count = N_V_BUFS;
        if (v4l2_ioctl(fd, VIDIOC_REQBUFS, &capi->rbuf) < 0) {
            int en = errno;
            error = "ioctl(VIDIOC_REQBUFS) failed: ";
            error += strerror(en);
            return -1;
        }
        capi->bufs = new capture_buf[capi->rbuf.count];
        memset(capi->bufs, 0, sizeof(capi->bufs[0]) * capi->rbuf.count);
        capi->vbufs = new v4l2_buffer[capi->rbuf.count];
        memset(capi->vbufs, 0, sizeof(capi->vbufs[0]) * capi->rbuf.count);
        for (size_t i = 0; i != capi->rbuf.count; ++i) {
            struct v4l2_buffer vbuf;
            memset(&vbuf, 0, sizeof(vbuf));
            vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            vbuf.memory = V4L2_MEMORY_MMAP;
            vbuf.index = i;
            if (v4l2_ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0) {
                int en = errno;
                error = "ioctl(VIDIOC_QUERYBUF) failed: ";
                error += strerror(en);
                return -1;
            }
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
        return 0;
    }

    capture_info *make_capture_info()
    {
        return (capture_info *)calloc(sizeof(capture_info), 1);
    }

    int enqueue_all_buffers(int fd, capture_info *ci)
    {
        for (size_t i = 0; i != ci->rbuf.count; ++i) {
            if (v4l2_ioctl(fd, VIDIOC_QBUF, &ci->vbufs[i]) < 0) {
                int en = errno;
                error = "ioctl(VIDIOC_QBUF) failed: ";
                error += strerror(en);
                return -1;
            }
        }
        return 0;
    }

    int start_capture(int fd, capture_info *ci)
    {
        int strm = 1;
        if (v4l2_ioctl(fd, VIDIOC_STREAMON, &strm) < 0) {
            int en = errno;
            error = "ioctl(VIDIOC_STREAMON) failed: ";
            error += strerror(en);
            return -1;
        }
        return 0;
    }

    int stop_capture(int fd, capture_info *ci)
    {
        int strm = 1;
        if (v4l2_ioctl(fd, VIDIOC_STREAMOFF, &strm) < 0) {
            int en = errno;
            error = "ioctl(VIDIOC_STREAMOFF) failed: ";
            error += strerror(en);
            return -1;
        }
        return 0;
    }

    int capture_one_frame_and_re_enqueue(int fd, capture_info *ci, void *dstJpg, size_t &ioSize)
    {
        v4l2_buffer vbuf;
        memset(&vbuf, 0, sizeof(vbuf));
        vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        vbuf.memory = V4L2_MEMORY_MMAP;
        if (v4l2_ioctl(fd, VIDIOC_DQBUF, &vbuf) < 0) {
            int en = errno;
            error = "ioctl(VIDIOC_DQBUF) failed: ";
            error += strerror(en);
            return -1;
        }

        /* process data here -- decode JPEG? dump to file? */
        unsigned int osz = vbuf.bytesused;
        if (osz > ioSize) {
            error = "capture: too small buffer provided";
            return -1;
        }
        ioSize = osz;
        if (dstJpg != 0) {
            memcpy(dstJpg, ci->bufs[vbuf.index].ptr, osz);
        }

        if (v4l2_ioctl(fd, VIDIOC_QBUF, &ci->vbufs[vbuf.index]) < 0) {
            int en = errno;
            error = "ioctl(VIDIOC_QBUF) failed: ";
            error += strerror(en);
            return -1;
        }
        return 0;
    }

    int close_video(int fd, capture_info *capi)
    {
        if (capi) {
            if (capi->bufs) {
                for (size_t i = 0; i != capi->rbuf.count; ++i) {
                    munmap(capi->bufs[i].ptr, capi->bufs[i].size);
                }
                delete[] capi->bufs;
            }
            if (capi->vbufs) {
                delete[] capi->vbufs;
            }
            free(capi);
        }
        if (fd >= 0) {
            close(fd);
        }
        return 0;
    }

}

