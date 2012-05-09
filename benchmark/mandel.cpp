
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <math.h>


unsigned char mandel[1000000];


unsigned int count_iters(double cx, double cy, unsigned int n) {
  double zx = 0;
  double zy = 0;
again:
  if (n >= 255) {
    return 0;
  }
  //  (zx + zy i) * (zx + zy i) + (cx + cy i)
  double nzx = zx * zx - zy * zy + cx;
  double nzy = 2 * zx * zy + cy;
  zx = nzx;
  zy = nzy;
  if (sqrt(zx * zx + zy * zy) > 2.0) {
    return n;
  }
  n += 1;
  goto again;
}

int main(int argc, char const *argv[]) {
  for (int j = 0; j < 1000; ++j) {
    for (int i = 0; i < 1000; ++i) {
      double cx = i / 250.0 - 2.0;
      double cy = j / 250.0 - 2.0;
      unsigned int niter = count_iters(cx, cy, 0);
      mandel[j * 1000 + i] = (unsigned char)niter;
    }
  }
  std::ofstream ofs(argv[1] ? argv[1] : "mandel.tga", std::ios_base::trunc | std::ios_base::binary);
  unsigned short hdr[9];
  unsigned int palette[256];
  for (int p = 0; p < 256; ++p) {
    unsigned int c = 0xff000000;
    c |= p;
    if (p > 128) {
      c |= ((p - 128) * 2 * 0x100) & 0xff00;
    }
    if (p > 192) {
      c |= ((p - 192) * 4 * 0x10000) & 0xff0000;
    }
    palette[p] = c;
  }
  hdr[0] = 14;
  hdr[1] = 2;
  hdr[2] = 0;
  hdr[3] = 0;
  hdr[4] = 0;
  hdr[5] = 0;
  hdr[6] = 1000;
  hdr[7] = 1000;
  hdr[8] = 0x20 | (0x8 << 8);
  ofs.write((char const *)hdr, 18);
  char zero[14] = { 0 };
  ofs.write(zero, 14);
  for (int i = 0; i < 1000000; ++i) {
    ofs.write((char const *)&palette[mandel[i]], 4);
  }
  ofs.close();
  return 0;
}

