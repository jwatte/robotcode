
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
  int n = 0;
  char ch = 0;
  int rate = 1;
  while (true) {
    if (read(fd, &ch, 1) != 1) {
      return 0;
    }
    if (ch < 32 || ch > 126) {
      char str[20];
      sprintf(str, "<0x%02x>", (unsigned char)ch);
      write(1, str, 6);
    }
    else {
      write(1, &ch, 1);
    }
    if ((++n & 0xf) == 0) {
      ch = rate++;
      write(fd, &ch, 1);
    }
  }
}
