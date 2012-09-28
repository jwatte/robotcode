#include "ay.h"
#include "color.h"
#include <stdio.h>

rgb a[] = {
    rgb(0, 0, 0),
    rgb(255, 255, 255),
    rgb(128, 128, 128),
    rgb(255, 0, 0),
    rgb(0, 255, 0),
    rgb(0, 0, 255),
    rgb(0, 255, 255),
    rgb(255, 0, 255),
    rgb(255, 255, 0),
    rgb(128, 0, 0),
    rgb(0, 128, 0),
    rgb(0, 0, 128),
    rgb(128, 255, 255),
    rgb(255, 128, 255),
    rgb(255, 255, 128),
    rgb(64, 160, 128),
    rgb(160, 128, 64),
    rgb(128, 64, 160),
};

int main() {
    for (size_t i = 0; i < sizeof(a)/sizeof(a[0]); ++i) {
        hcl h(a[i]);
        rgb o(h);
        fprintf(stdout, "%02x %02x %02x -> %3d %3d %3d -> %02x %02x %02x\n",
            a[i].r, a[i].g, a[i].b, h.h, h.c, h.l, o.r, o.g, o.b);
    }
    return 0;
}
