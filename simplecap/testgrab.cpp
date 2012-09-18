
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


extern "C" {

int open_dev(std::string const &str)
{
  int fd = v4l2_open(str.c_str(), O_RDWR);
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
  fmt.fmt.pix.width = 1280;
  fmt.fmt.pix.height = 720;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  /*
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24;
  */
  fmt.fmt.pix.field = V4L2_FIELD_ANY;
  fmt.fmt.pix.bytesperline = 1280*720*3;
  fmt.fmt.pix.sizeimage = 1280*720*3;
  if (v4l2_ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
    perror("ioctl(VIDIOC_S_FMT) failed");
    exit(1);
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

void config_dev(int fd, capture_info *capi)
{
  memset(capi, 0, sizeof(*capi));
  capi->rbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  capi->rbuf.memory = V4L2_MEMORY_MMAP;
  capi->rbuf.count = 2;
  if (v4l2_ioctl(fd, VIDIOC_REQBUFS, &capi->rbuf) < 0) {
    perror("ioctl(VIDIOC_REQBUFS) failed");
    exit(1);
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
      perror("ioctl(VIDIOC_QUERYBUF) failed");
      exit(1);
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
      perror("mmap() failed");
      exit(1);
    }
    capi->vbufs[i] = vbuf;
  }
}

void enqueue_all_buffers(int fd, capture_info *ci)
{
  for (size_t i = 0; i != ci->rbuf.count; ++i) {
    if (v4l2_ioctl(fd, VIDIOC_QBUF, &ci->vbufs[i]) < 0) {
      perror("ioctl(VIDIOC_QBUF) failed");
      exit(1);
    }
  }
}

void start_capture(int fd, capture_info *ci)
{
  int strm = 1;
  if (v4l2_ioctl(fd, VIDIOC_STREAMON, &strm) < 0) {
    perror("ioctl(VIDIOC_STREAMON) failed");
    exit(1);
  }
}

void capture_one_frame_and_re_enqueue(int fd, capture_info *ci, int ofd)
{
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
  write(ofd, &osz, sizeof(osz));
  write(ofd, ci->bufs[vbuf.index].ptr, osz);
  if (v4l2_ioctl(fd, VIDIOC_QBUF, &ci->vbufs[vbuf.index]) < 0) {
    perror("ioctl(VIDIOC_QBUF) failed");
    exit(1);
  }
}

}


volatile bool running = true;

void sig_handler(int)
{
  running = false;
}

extern int main2(int argc, char const *argv[]);

int main(int argc, char const *argv[])
{
  if (argv[1] && !strcmp(argv[1], "--dump")) {
    return main2(argc-1, argv+1);
  }
  timeval tv_start, tv_now;
  int fd = open_dev(argv[1] ? argv[1] : "/dev/video0");
  char const *ofn = (argv[1] && argv[2]) ? argv[2] : "output.cap";
  int ofd = open(ofn, O_RDWR | O_CREAT | O_TRUNC, 0664);
  if (ofd < 0) {
    perror(ofn);
    exit(1);
  }
  char hd[4] = { 'c', 'a', 'p', 0 };
  write(ofd, hd, 4);
  capture_info capi;
  config_dev(fd, &capi);
  enqueue_all_buffers(fd, &capi);
  int n = 0;
  signal(SIGINT, &sig_handler);
  start_capture(fd, &capi);
  gettimeofday(&tv_start, 0);
  while (running) {
    capture_one_frame_and_re_enqueue(fd, &capi, ofd);
    std::cerr << n << "\r";
    ++n;
  }
  gettimeofday(&tv_now, 0);
  std::cerr << "... done" << std::endl;
  ::close(ofd);
  double seconds = tv_now.tv_sec + tv_now.tv_usec * 1e-6 - tv_start.tv_sec - tv_start.tv_usec * 1e-6;
  std::cerr << "captured " << n << " frames in " << seconds << " seconds." << std::endl;
  std::cerr << "frame rate: " << n / seconds << " fps." << std::endl;
  return 0;
}

