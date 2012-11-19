
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

static int fd;

struct capture_buf {
  void *ptr;
  size_t size;
};

struct capture_info {
  v4l2_requestbuffers rbuf;
  v4l2_buffer *vbufs;
  capture_buf *bufs;
};

static capture_info cinfo;

static void config_dev(capture_info *capi, size_t size)
{
  fprintf(stderr, "config_dev()\n");
  memset(capi, 0, sizeof(*capi));
  capi->rbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  capi->rbuf.memory = V4L2_MEMORY_MMAP;
  capi->rbuf.count = 2;
  if (v4l2_ioctl(fd, VIDIOC_REQBUFS, &capi->rbuf) < 0) {
    perror("ioctl(VIDIOC_REQBUFS) failed");
    exit(1);
  }
  fprintf(stderr, "reqbufs count: %d\n", capi->rbuf.count);
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
    vbuf.length = size;
    if (v4l2_ioctl(fd, VIDIOC_QUERYBUF, &vbuf) < 0) {
      perror("ioctl(VIDIOC_QUERYBUF) failed");
      exit(1);
    }
    capi->bufs[i].size = vbuf.length;
    capi->bufs[i].ptr = v4l2_mmap(
      0, 
      vbuf.length, 
      PROT_READ | PROT_WRITE, 
      MAP_SHARED, 
      fd, 
      vbuf.m.offset);
    if (MAP_FAILED == capi->bufs[i].ptr) {
      perror("mmap() failed");
      fprintf(stderr, "index: %d\nlength: %x\nfd: %d\noffset: %x\n", vbuf.index, vbuf.length, fd, vbuf.m.offset);
      exit(1);
    }
    capi->vbufs[i] = vbuf;
  }
}

void open_dev(std::string const &str, int width, int height)
{
  fprintf(stderr, "open_dev(%s, %d, %d)\n", str.c_str(), width, height);
  fd = v4l2_open(str.c_str(), O_RDWR);
  if (fd < 0) {
    perror(str.c_str());
    exit(1);
  }
  struct v4l2_capability cap;
  if (v4l2_ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
    perror("ioctl(VIDIOC_QUERYCAP) failed");
    exit(1);
  }
  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    perror("device does not support mmap() streaming");
    exit(1);
  }
  struct v4l2_format fmt;
  memset(&fmt, 0, sizeof(fmt));
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = width;
  fmt.fmt.pix.height = height;
  fmt.fmt.pix.field = V4L2_FIELD_ANY;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  fmt.fmt.pix.bytesperline = width*3;
  fmt.fmt.pix.sizeimage = width*height*3;
  /*
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.bytesperline = width*2;
  fmt.fmt.pix.sizeimage = width*height*2;
  /**/
  if (v4l2_ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
    perror("ioctl(VIDIOC_S_FMT) failed");
    exit(1);
  }

  config_dev(&cinfo, width * height * 2);
}

void close_dev() {
  fprintf(stderr, "close_dev()\n");
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}


static void enqueue_all_buffers(capture_info *ci)
{
  fprintf(stderr, "enqueue_all_buffers(%d)\n", ci->rbuf.count);
  for (size_t i = 0; i != ci->rbuf.count; ++i) {
    if (v4l2_ioctl(fd, VIDIOC_QBUF, &ci->vbufs[i]) < 0) {
      perror("ioctl(VIDIOC_QBUF) failed");
      exit(1);
    }
  }
}

void start_capture()
{
  fprintf(stderr, "start_capture()\n");
  enqueue_all_buffers(&cinfo);
  int strm = 1;
  if (v4l2_ioctl(fd, VIDIOC_STREAMON, &strm) < 0) {
    perror("ioctl(VIDIOC_STREAMON) failed");
    exit(1);
  }
}

void stop_capture()
{
  fprintf(stderr, "stop_capture()\n");
  int strm = 1;
  if (v4l2_ioctl(fd, VIDIOC_STREAMOFF, &strm) < 0) {
    perror("ioctl(VIDIOC_STREAMOFF) failed");
    exit(1);
  }
}

void capture_frame(void *&outptr, size_t &out_size)
{
  fprintf(stderr, "capture_frame()\n");
  auto ci = &cinfo;
  v4l2_buffer vbuf;
  memset(&vbuf, 0, sizeof(vbuf));
  vbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  vbuf.memory = V4L2_MEMORY_MMAP;
  if (v4l2_ioctl(fd, VIDIOC_DQBUF, &vbuf) < 0) {
    perror("ioctl(VIDIOC_DQBUF) failed");
    exit(1);
  }
  /* process data here */
  unsigned int osz = vbuf.bytesused;
  outptr = ci->bufs[vbuf.index].ptr;
  out_size = osz;
  if (v4l2_ioctl(fd, VIDIOC_QBUF, &ci->vbufs[vbuf.index]) < 0) {
    perror("ioctl(VIDIOC_QBUF) failed");
    exit(1);
  }
}






