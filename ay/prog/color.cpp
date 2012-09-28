
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

void vote_hcl(ImagePtr &img, bool outhcl) {
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

struct pix_span {
    pix_span(int l, int r) :
        left(l),
        right(r),
        previx(-1),
        height(0)
    {
    }
    int left;
    int right;
    int previx;
    int height;
};

struct span_info {
    span_info() :
        prev_row_start(0),
        cur_row_start(0) {
    }
    std::vector<pix_span> spans;
    int prev_row_start;
    int cur_row_start;
    static void overlap(pix_span a, pix_span b) {
        int leftmost = std::max(a.left, b.left);
        int rightmost = std::min(a.right, b.right);
        return rightmost - leftmost;
    }
    void add_span(pix_span ps) {
        pix_span const *prev = spans.size() ? &spans[0] : 0;
        int match = -1;
        for (int i = prev_row_start; i < cur_row_start; ++i) {
            if (prev[i].left < ps.right && prev[i].right > ps.left) {
                if (ps.height < prev[i].height) {
                    //  clearly a better match
                    ps.height = prev[i].height;
                    match = i;
                }
                else if (ps.height == prev[i].height) {
                    assert(match != -1);
                    //  equally good match -- use biggest overlap
                    if (overlap(ps, prev[i]) > overlap(ps, prev[match])) {
                        ps.height = prev[i].height;
                        match = i;
                    }
                }
                //  else do nothing
            }
        }
        ps.previx = match;
        spans.push_back(ps);
    }
    void end_row() {
        prev_row_start = cur_row_start;
        cur_row_start = spans.size();
    }
};

//  Find the tallest contiguous vertical areas of pixels of a 
//  certain color.
//
//  parameters:
//  img -- the image to search; should be in hcl color space
//  target -- the color to search for
//  tolerance -- the amount of deviation to accept (inclusive)
//  allowed_gap -- how many pixels from previous pixel to still 
//      count as a contiguous match. (1 == no gaps allowed)
//  max_len -- the maximum length of a horizontal span of pixels, 
//      used to break a wide, horizontal, line of the right color
//      up into separate pieces for robustness.
void find_cones(
    ImagePtr img,
    hcl const target,
    hcl const tolerance,
    int allowed_gap = 2,
    int max_len = 50) {

    size_t rb = img->rowbytes(), h = img->height(), w = img->width();
    unsigned char *ptr = img->data(), end = ptr + rb * h;
    span_info spans;
    spans.prev_row_start = -1;
    spans.cur_row_start = -1;
    while (end > ptr) {
        end -= rb;
        unsigned char const *pix = end;
        int span = 0;
        int start = -1;
        int len = 0;
        for (int i = 0; i < w; ++i) {
            unsigned char h = *pix++;
            unsigned char c = *pix++;
            unsigned char l = *pix++;
            if ((unsigned char)(target.h - h) <= tolerance.h ||
                (unsigned char)(h - target.h) <= tolerance.h) {
                if ((unsigned char)(target.c - c) <= tolerance.c ||
                    (unsigned char)(c - target.c) <= tolerance.c) {
                    if ((unsigned char)(target.l - l) <= tolerance.l ||
                        (unsigned char)(l - target.l) <= tolerance.l) {
                        span = allowed_gap;
                        if (start < 0) {
                            start = i;
                        }
                    }
                }
            }
            if (span > 0 && len < max_len) {
                ++len;
                --span;
            }
            else if (start) {
                pix_span ps(start, start + len - allowed_gap + 1);
                spans.add_span(ps);
            }
        }
        if (start != -1) {
            pix_span ps(start, start + len - allowed_gap + 1);
            spans.add_span(ps);
        }
        spans.end_row();
    }
    //  now, find the highest spans
}

int main(int argc, char const *argv[]) {
  try {
    if (argc != 3) {
      throw std::runtime_error("usage: j2t input.jpg output.tga");
    }
    ImagePtr img(load_image(argv[1]));
    vote_hcl(img, true);
    find_cones(img, hcl(8, 144, 144), hcl(16, 32, 32), 2, 40);
    save_image(img, argv[2]);
  }
  catch (std::exception const &err) {
    std::cerr << err.what() << std::endl;
    return 1;
  }
  return 0;
}

