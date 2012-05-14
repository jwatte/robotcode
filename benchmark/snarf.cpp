
#include <iostream>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <termios.h>

int _err(int r, char const *expr, char const *file, int line) {
  if (r >= 0) return r;
  perror(expr);
  fprintf(stderr, "\n%s:%d: %s returns %d\n", file, line, expr, r);
  exit(1);
}


#define err(x) \
  _err(x,#x,__FILE__,__LINE__)

void do_fatal(char *buf, size_t size)
{
  fprintf(stderr, "FATAL: %d (0x%02x)\n", (unsigned char)buf[1], (unsigned char)buf[1]);
}

void do_open(char *buf, size_t size)
{
  fprintf(stderr, "ONLINE\n");
}

void do_data(char *buf, size_t size)
{
  fprintf(stderr, "From node %d: %d bytes:", (unsigned char)buf[1], (unsigned char)buf[2]);
  for (size_t i = 3; i != size; ++i) {
    if (buf[i] > 32 && buf[i] < 127) {
      fprintf(stderr, " %c", buf[i]);
    }
    else {
      fprintf(stderr, " <0x%02x>", (unsigned char)buf[i]);
    }
  }
  fprintf(stderr, "\n");
}

void do_nak(char *buf, size_t size)
{
  fprintf(stderr, "From node %d: NAK\n", (unsigned char)buf[1]);
}

void do_dist(char *buf, size_t size)
{
  fprintf(stderr, "From distance sensor %d: distance 0x%02x\n", 
    (unsigned char)buf[1], (unsigned char)buf[2]);
}

struct FormatTypes {
  char type;
  char length;
  char lengthOffset;
  void (*func)(char *, size_t size);
};
FormatTypes formats[] = {
  { 'F', 2, 0, &do_fatal },
  { 'O', 1, 0, &do_open },
  { 'D', 3, 2, &do_data },
  { 'N', 2, 0, &do_nak },
  { 'R', 3, 0, &do_dist },
};

void maybe_decode(char *buf, int &ptr) {
  if (ptr == 0) {
    return;
  }
  for (size_t i = 0; i != sizeof(formats)/sizeof(formats[0]); i++) {
    if (buf[0] == formats[i].type) {
      size_t len = formats[i].length;
      if (ptr < len) {
        return;
      }
      if (formats[i].lengthOffset) {
        len += buf[formats[i].lengthOffset];
      }
      if (ptr >= len) {
        (*formats[i].func)(buf, len);
        memmove(buf, buf + len, ptr - len);
        ptr -= len;
      }
      return;
    }
  }
  fprintf(stderr, "error: unknown command 0x%02x\n", (unsigned char)*buf);
  memmove(buf, buf+1, ptr-1);
  --ptr;
}


int main(int argc, char const *argv[]) {
  if (argc != 2 || argv[1][0] == '-') {
    std::cerr << "usage: snarf /dev/ttyACM0" << std::endl;
    return 1;
  }
  int fd = open(argv[1], O_RDWR);
  if (fd < 0) {
    perror(argv[1]);
    return 1;
  }
  struct termios tio;
  memset(&tio, 0, sizeof(tio));
  err(tcgetattr(fd, &tio));
  cfmakeraw(&tio);
  tio.c_iflag = IGNBRK | IGNPAR;
  tio.c_oflag = 0;
  tio.c_cflag = CLOCAL | CREAD | CS8;
  tio.c_cc[VMIN] = 1;
  tio.c_cc[VTIME] = 0;
  cfsetispeed(&tio, B115200);
  cfsetospeed(&tio, B115200);
  err(tcsetattr(fd, TCSANOW, &tio));
  char ch = 0;
  char cmd[100];
  int ptr = 0;
  while (true) {
    if (read(fd, &ch, 1) != 1) {
      fprintf(stderr, "error: end of input\n");
      return 0;
    }
    cmd[ptr++] = ch;
    maybe_decode(cmd, ptr);
    if (ptr == 100) {
      fprintf(stderr, "error: improper framing (too long)\n");
      ptr = 0;
    }
  }
}
