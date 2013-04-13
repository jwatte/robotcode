
#include <iostream>

#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>


int main2(int argc, char const *argv[])
{
  char const *ifn = argv[1] ? argv[1] : "output.cap";
  int fd = open(ifn, O_RDONLY);
  if (fd < 0) {
    perror(ifn);
    exit(1);
  }
  char hd[4];
  read(fd, hd, 4);
  if (hd[0] != 'c' || hd[1] != 'a' || hd[2] != 'p' || hd[3] != 0) {
    fprintf(stderr, "%s: not a cap0 file\n", ifn);
    exit(1);
  }
  int n = 0;
  while (true) {
    unsigned int sz = 0;
    if (4 > read(fd, &sz, 4)) {
      break;
    }
    char *buf = (char *)malloc(sz);
    if (!buf) {
      perror("out of memory");
      exit(1);
    }
    read(fd, buf, sz);
    char path[1024];
    sprintf(path, "%s.%05d.jpg", ifn, n);
    int ofd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0664);
    if (ofd < 0) {
      perror(path);
      exit(1);
    }
    write(ofd, buf, sz);
    close(ofd);
    ++n;
    free(buf);
  }
  std::cerr << "extracted " << n << " images from " << ifn << std::endl;
  return 0;
}

