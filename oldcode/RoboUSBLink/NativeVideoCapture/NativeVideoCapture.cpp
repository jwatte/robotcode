
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

/*

int open_dev(char const *devName);
capture_info *make_capture_info();
int config_dev(int fd, void *capi);
int enqueue_all_buffers(int fd, capture_info *ci);
int start_capture(int fd, capture_info *ci);
int capture_one_frame_and_re_enqueue(int fd, capture_info *ci);
int stop_capture(int fd, capture_info *ci);
int close_video(int fd, capture_info *capi);
char const *get_error();

*/

extern "C" {

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
  capi->rbuf.count = 2;
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
  for (int i = 0; i != ci->rbuf.count; ++i) {
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

int capture_one_frame_and_re_enqueue(int fd, capture_info *ci)
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
      for (int i = 0; i != capi->rbuf.count; ++i) {
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

