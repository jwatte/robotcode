
#include "ay.h"
#include <math.h>
#include <string.h>
#include <stdexcept>


struct hcl;
struct rgb;

enum {
    RED_MAJOR = 0,
    GREEN_MAJOR = 85,
    BLUE_MAJOR = 170
};
int drgbfunc[256 * 3];

class init_func {
public:
    init_func() {
        for (int i = 0; i < 256; ++i) {
            drgbfunc[i * 3] = (int)(170 * cosf(i * M_PI / 128));
        }
        for (int i = 0; i < 256; ++i) {
            drgbfunc[i * 3 + 1] = drgbfunc[((i + 85) & 255) * 3];
            drgbfunc[i * 3 + 2] = drgbfunc[((i - 85) & 255) * 3];
        }
    }
};
init_func inif;

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
};

static const float sqrt3 = sqrtf(3.0f);
static const float scale510 = 1.0f / 510;
static const float pi2 = M_PI * 2;

rgb::operator hcl() const {
    float alpha = (2*r - g - b) * scale510;
    float beta = (sqrt3 * (g - b)) * scale510;
    float hue = M_PI, chroma = 0;
    if (alpha != 0 || beta != 0) {
        hue = atan2(beta, alpha);
        if (hue < 0) {
            hue += pi2;
        }
        chroma = sqrtf(alpha * alpha + beta * beta);
        if (chroma > 1) {
            chroma = 1;
        }
    }
    unsigned char lightness = (std::min(r, std::min(g, b)) +
        std::max(r, std::max(g, b))) >> 1;
    return hcl((unsigned char)(hue * 255 / pi2), 
        (unsigned char)(chroma * 255),
        lightness);
}

struct Diamond {
    Diamond() {
        memset(cnt, 0, sizeof(cnt));
        memset(cnt_skew, 0, sizeof(cnt_skew));
    }
    //  skew is rotated halfway between colors compared to non-skew
    unsigned char cnt[5+12+16];
    unsigned char cnt_skew[5+12+16];
    void add(hcl value) {
        cnt[to_cell(value)]++;
        cnt_skew[to_cell(hcl(value.h+8, value.c, value.l))]++;
    }
    //  vote on what color the pixel is
    hcl classify() {
        unsigned char mv = 0;
        unsigned char top = 0;
        for (int i = 0; i < 33; ++i) {
            if (cnt[i] > mv) {
                mv = cnt[i];
                top = i;
            }
            if (cnt_skew[i] > mv) {
                mv = cnt_skew[i];
                top = i + 33;
            }
        }
        if (top < 33) {
            return get_cell_color(top);
        }
        hcl pix(get_cell_color(top - 33));
        pix.h -= 8;
        return pix;
    }
    hcl get_cell_color(unsigned char cell) {
        if (cell < 5) {
            //  grayscale
            return hcl(0, 0, cell * 50 + 25);
        }
        if (cell < 17) {
            return hcl(
                ((cell - 5) & 3) * 64 + 32, 
                64,
                ((cell - 5) / 4) * 96 + 32);
        }
        return hcl(
                (cell - 17) * 16 + 8,
                160,
                160);
    }
    unsigned char to_cell(hcl val) {
        if (val.c < 24) {
            //  gray axis
            if (val.l >= 250) return 4;
            return val.l / 50;
        }
        else if (val.c < 48) {
            //  intermediate three-layer four-corner cube
            return 5 + val.h / 64 + (val.l < 96 ? 0 : val.l > 160 ? 2 : 1) * 4;
        }
        else {
            //  hue driven
            return 17 + val.h / 16;
        }
    }
};

void process(ImagePtr &img) {
    ImagePtr op(Image::create(img->width() / 8, img->height() / 8));
    for (size_t y = 0; y < op->height(); ++y) {
        for (size_t x = 0; x < op->width(); ++x) {
            Diamond d;
            unsigned char *ptr = img->data() + y * 8 * img->rowbytes() + x * 8 * 3;
            for (size_t a = 0; a < 8; ++a) {
                for (size_t b = 0; b < 8; ++b) {
                    rgb pix(ptr[b * 3 + 0], ptr[b * 3 + 1], ptr[b * 3 + 2]);
                    d.add(pix);
                }
                ptr += img->rowbytes();
            }
            hcl cls(d.classify());
            rgb result(cls);
            unsigned char *dst = op->data() + y * op->rowbytes();
            dst[x * 3] = cls.h; //result.r;
            dst[x * 3 + 1] = cls.c; //result.g;
            dst[x * 3 + 2] = cls.l; //result.b;
        }
    }
    img = op;
}

int main(int argc, char const *argv[]) {
  try {
    if (argc != 3) {
      throw std::runtime_error("usage: j2t input.jpg output.tga");
    }
    ImagePtr img(load_image(argv[1]));
    process(img);
    save_image(img, argv[2]);
  }
  catch (std::exception const &err) {
    std::cerr << err.what() << std::endl;
    return 1;
  }
  return 0;
}

