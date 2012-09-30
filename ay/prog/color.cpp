
#include "ay.h"
#include "color.h"
#include <math.h>
#include <string.h>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <sstream>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>


struct found_rect : rect {
    found_rect(int six, int left, int top, int right, int bottom) :
        rect(left, top, right, bottom),
        spanix(six),
        score(0) {
    }
    int spanix;
    float score;
};

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
    pix_span(int t, int l, int r) :
        top(t),
        left(l),
        right(r),
        previx(-1),
        height(0)
    {
    }
    int top;
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
    static int overlap(pix_span a, pix_span b) {
        int leftmost = std::max(a.left, b.left);
        int rightmost = std::min(a.right, b.right);
        return rightmost - leftmost;
    }
    void add_span(pix_span ps) {
        pix_span const *prev = spans.size() ? &spans[0] : 0;
        int match = -1;
        ps.height = 1;
        for (int i = prev_row_start; i < cur_row_start; ++i) {
            if (prev[i].left < ps.right && prev[i].right > ps.left) {
                if (ps.height <= prev[i].height) {
                    //  clearly a better match
                    ps.height = prev[i].height + 1;
                    match = i;
                }
                else if (ps.height == prev[i].height + 1) {
                    assert(match != -1);
                    //  equally good match -- use biggest overlap
                    if (overlap(ps, prev[i]) > overlap(ps, prev[match])) {
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

std::string str(pix_span const &ps) {
    std::stringstream ss;
    ss << ps.left << "," << ps.top << "-" << ps.right << "," << (ps.top + ps.height);
    return ss.str();
}

std::string str(rect const &r) {
    std::stringstream ss;
    ss << r.x1 << "," << r.y1 << "-" << r.x2 << "," << r.y2;
    return ss.str();
}

template<typename R>
bool overlaps_previous(pix_span const &ps, R const &prev) {
    for (typename R::const_iterator ptr(prev.begin()), end(prev.end());
        ptr != end; ++ptr) {
        if (ps.top < (*ptr).y2 && ps.top + ps.height > (*ptr).y1 &&
            ps.left < (*ptr).x2 && ps.right > (*ptr).x1) {
            //std::cout << "overlaps previous: " << str(ps) << " == " << str(*ptr) << std::endl;
            return true;
        }
    }
    //std::cout << "no overlap previous: " << str(ps) << std::endl;
    return false;
}

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
    std::vector<found_rect> &cones,
    hcl const target,
    hcl const tolerance,
    float threshold = 0.1f,
    float desired_ratio = 2.5f,
    int allowed_gap = 2,
    int max_len = 40) {

    size_t rb = img->rowbytes(), h = img->height(), w = img->width();
    unsigned char *ptr = img->data(), *end = ptr + rb * h;
    span_info spans;
    spans.prev_row_start = 0;
    spans.cur_row_start = 0;
    int ih = h;
    while (end > ptr) {
        end -= rb;
        --ih;
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
            else if (start > -1) {
                pix_span ps(ih, start, start + len - allowed_gap + 1);
                spans.add_span(ps);
                start = -1;
                len = 0;
            }
        }
        if (start != -1) {
            pix_span ps(ih, start, start + len - allowed_gap + 1);
            spans.add_span(ps);
        }
        spans.end_row();
    }
    //  now, find the highest spans
    size_t n = spans.spans.size();
    std::vector<found_rect> found;
    while (n > 0) {
        --n;
        pix_span ps(spans.spans[n]);
        if (ps.height > 3 && !overlaps_previous(ps, found)) {
            found_rect r(n, ps.left, ps.top, ps.right, ps.top + ps.height);
            float sd = 1.0 / ps.height;
            //std::cout << "ps.height " << ps.height << " score delta " << sd << std::endl;
            int prevx1 = r.x1;
            int prevx2 = r.x2;
            while (ps.height > 1) {
                pix_span old(ps);
                ps = spans.spans[ps.previx];
                assert(old.left < ps.right && old.right > ps.left);
                assert(old.height == ps.height + 1);
                assert(old.top == ps.top - 1);
                if (ps.left > prevx1) {
                    r.score -= sd;
                    //std::cout << "-left " << ps.left << " > " << prevx1 << std::endl;
                }
                else if (ps.left < prevx1) {
                    r.score += sd;
                    if (ps.left < r.x1) {
                        r.x1 = ps.left;
                    }
                    //std::cout << "+left " << ps.left << " < " << prevx1 << std::endl;
                }
                prevx1 = ps.left;
                if (ps.right < prevx2) {
                    r.score -= sd;
                    //std::cout << "-right " << ps.right << " < " << prevx2 << std::endl;
                }
                else if (ps.right > prevx2) {
                    r.score += sd;
                    if (ps.right > r.x2) {
                        r.x2 = ps.right;
                    }
                    //std::cout << "+right " << ps.right << " > " << prevx2 << std::endl;
                }
                prevx2 = ps.right;
            }
            //  ratio should be about 1:2, so penalize those that are off
            float ratio = (float)r.height() / r.width();
            //std::cout << "ratio " << ratio << " desired " << desired_ratio
            //     << std::endl;
            if (ratio < desired_ratio) {
                //std::cout << "adjusting by " << ratio / desired_ratio
                //    << " for too wide." << std::endl;
                r.score = r.score * ratio / desired_ratio;
            }
            else {
                //std::cout << "adjusting by " << desired_ratio / ratio
                //    << " for too tall." << std::endl;
                r.score = r.score * desired_ratio / ratio;
            }
            r.score = (r.score * r.height() / r.width()) * desired_ratio;
            //std::cout << "score " << r.score << " " << str(r) << std::endl;
            found.push_back(r);
        }
    }
    BOOST_FOREACH(auto x, found) {
        if (x.score >= threshold) {
            cones.push_back(x);
        }
    }
}

template<typename Iter, typename Callable>
void frame_rects(ImagePtr img, Iter ptr, Iter end, Callable fn) {
    unsigned char *base = img->data();
    size_t rb = img->rowbytes();
    size_t h = img->height();
    size_t w = img->width();
    while (ptr < end) {
        auto c = fn(*ptr);
        unsigned char *cc = (unsigned char *)&c;
        //std::cout << "framing " << (*ptr).x1 << "," << (*ptr).y1 << "-"
        //    << (*ptr).x2 << "," << (*ptr).y2 << " color "
        //    << (int)cc[0] << "," << (int)cc[1] << "," << (int)cc[2] << std::endl;
        size_t left = (*ptr).x1;
        if (left > w) left = w;
        size_t top = (*ptr).y1;
        if (top > h) top = h;
        size_t right = (*ptr).x2;
        if (right > w) right = w;
        if (right < left) right = left;
        size_t bottom = (*ptr).y2;
        if (bottom > h) bottom = h;
        if (bottom < top) bottom = top;
        if (right > left && bottom > top) {
            unsigned char *bb = base + rb * top + left * 3;
            for (size_t p = left; p < right; ++p) {
                bb[0] = cc[0];
                bb[1] = cc[1];
                bb[2] = cc[2];
                bb += 3;
            }
            bb = base + rb * top + left * 3;
            for (size_t p = top; p < bottom; ++p) {
                bb[0] = cc[0];
                bb[1] = cc[1];
                bb[2] = cc[2];
                bb += rb;
            }
            bb = base + rb * top + (right - 1) * 3;
            for (size_t p = top; p < bottom; ++p) {
                bb[0] = cc[0];
                bb[1] = cc[1];
                bb[2] = cc[2];
                bb += rb;
            }
            bb = base + rb * (bottom - 1) + left * 3;
            for (size_t p = left; p < right; ++p) {
                bb[0] = cc[0];
                bb[1] = cc[1];
                bb[2] = cc[2];
                bb += 3;
            }
        }
        ++ptr;
    }
}

hcl color_for_rect(found_rect const &fr) {
    float f = std::min(1.0f, std::max(fr.score, 0.0f));
    return hcl(0, 0, (unsigned char)(f * 255));
}

int main(int argc, char const *argv[]) {
  try {
    if (argc != 3) {
      throw std::runtime_error("usage: j2t input.jpg output.tga");
    }
    ImagePtr img(load_image(argv[1]));
    vote_hcl(img, true);
    std::vector<found_rect> cones;
    find_cones(img, cones, hcl(8, 144, 144), hcl(16, 32, 32));
    frame_rects(img, cones.begin(), cones.end(), color_for_rect);
    convert<hcl, rgb>(img);
    save_image(img, argv[2]);
  }
  catch (std::exception const &err) {
    std::cerr << err.what() << std::endl;
    return 1;
  }
  return 0;
}

