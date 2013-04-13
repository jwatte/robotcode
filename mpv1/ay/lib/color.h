#if !defined(ay_color_h)
#define ay_color_h

#include "ay.h"
#include <math.h>
#include <algorithm>

struct hcl;
struct rgb;

enum {
    RED_HUE = 0,
    GREEN_HUE = 85,
    BLUE_HUE = 170
};
inline unsigned char clamp(int ch) {
    if (ch < 0) return 0;
    if (ch > 255) return 255;
    return ch;
}

struct rgb {
    rgb(unsigned char ir, unsigned char ig, unsigned char ib) :
        r(ir),
        g(ig),
        b(ib) {
    }
    unsigned char r;
    unsigned char g;
    unsigned char b;
    operator hcl() const;
    static const float sqrt3;
    static const float scale510;
    static const float pi2;

};

struct hcl {
    hcl(unsigned char ih, unsigned char ic, unsigned char il) :
        h(ih),
        c(ic),
        l(il) {
    }
    unsigned char h;
    unsigned char c;
    unsigned char l;
    operator rgb() const {
        return rgb(clamp(l + (drgbfunc[h * 3] * c >> 8)),
            clamp(l + (drgbfunc[h * 3 + 1] * c >> 8)),
            clamp(l + (drgbfunc[h * 3 + 2] * c >> 8)));
    }
    static int drgbfunc[];
};

template<typename From, typename To>
void convert(ImagePtr img) {
    size_t rb = img->rowbytes();
    size_t w = img->width();
    size_t h = img->height();
    From f(0, 0, 0);
    To t(0, 0, 0);
    for (size_t y = 0; y != h; ++y) {
        unsigned char *ptr = img->data() + y * rb;
        for (size_t x = 0; x != w; ++x) {
            memcpy(&f, ptr, 3);
            t = f;
            memcpy(ptr, &t, 3);
            ptr += 3;
        }
    }
}

#endif

