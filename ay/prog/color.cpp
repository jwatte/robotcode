
#include "ay.h"
#include "color.h"
#include <math.h>
#include <string.h>
#include <stdexcept>


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
        if (val.c < 8) {
            //  gray axis
            if (val.l >= 250) return 4;
            return val.l / 50;
        }
        else if (val.c < 40) {
            //  intermediate three-layer four-corner cube
            return 5 + val.h / 64 + (val.l < 96 ? 0 : val.l > 160 ? 2 : 1) * 4;
        }
        else {
            //  hue driven
            return 17 + val.h / 16;
        }
    }
};

void process(ImagePtr &img, bool outhcl) {
    int div = 8;
    if (img->width() < 1400) {
        //  try to keep the output size approximately the same
        div = 6;
    }
    if (img->width() < 1000) {
        //  below 4, the "voting" gets VERY noisy
        div = 4;
    }
    ImagePtr op(Image::create(img->width() / div, img->height() / div));
    for (size_t y = 0; y < op->height(); ++y) {
        for (size_t x = 0; x < op->width(); ++x) {
            Diamond d;
            unsigned char *ptr = img->data() + y * div * img->rowbytes() + x * div * 3;
            for (size_t a = 0; a < div; ++a) {
                for (size_t b = 0; b < div; ++b) {
                    rgb pix(ptr[b * 3 + 0], ptr[b * 3 + 1], ptr[b * 3 + 2]);
                    d.add(pix);
                }
                ptr += img->rowbytes();
            }
            hcl cls(d.classify());
            unsigned char *dst = op->data() + y * op->rowbytes();
            if (outhcl) {
                dst[x * 3] = cls.h;
                dst[x * 3 + 1] = cls.c;
                dst[x * 3 + 2] = cls.l;
            }
            else {
                rgb result(cls);
                dst[x * 3] = result.r;
                dst[x * 3 + 1] = result.g;
                dst[x * 3 + 2] = result.b;
            }
        }
    }
    img = op;
}

int main(int argc, char const *argv[]) {
  try {
    bool outhcl = false;
    if (argv[1] && !strcmp(argv[1], "--hcl")) {
        outhcl = true;
        --argc;
        ++argv;
    }
    if (argc != 3) {
      throw std::runtime_error("usage: j2t [--hcl] input.jpg output.tga");
    }
    ImagePtr img(load_image(argv[1]));
    process(img, outhcl);
    save_image(img, argv[2]);
  }
  catch (std::exception const &err) {
    std::cerr << err.what() << std::endl;
    return 1;
  }
  return 0;
}

