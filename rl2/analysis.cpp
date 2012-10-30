
#include "analysis.h"
#include <string.h>
#include <boost/foreach.hpp>
#include <iostream>



void Pixmap::color_correct() {
    //  gather statistics
    double sum[3] = { 0, 0, 0 };
    double sumsq[3] = { 0, 0, 0 };
    unsigned char min[3] = { 255, 255, 255 };
    unsigned char max[3] = { 0, 0, 0 };
    std::vector<unsigned char> oot;
    oot.resize(rowbytes * height);
    for (int row = 0, nrow = height; row != nrow; ++row) {
        unsigned char const *src = bits + rowbytes * row;
        for (int col = 0, ncol = width; col != ncol; ++col) {
            min[0] = std::min(min[0], src[0]);
            max[0] = std::max(max[0], src[0]);
            sum[0] += src[0];
            sumsq[0] += (double)src[0] * src[0];
            min[1] = std::min(min[1], src[1]);
            max[1] = std::max(max[1], src[1]);
            sum[1] += src[1];
            sumsq[1] += (double)src[1] * src[1];
            min[2] = std::min(min[2], src[2]);
            max[2] = std::max(max[2], src[2]);
            sum[2] += src[2];
            sumsq[2] += (double)src[2] * src[2];
            src += Image::BytesPerPixel;
        }
    }

    //  now I have mean, standard deviation, min, and max
    double num = width * (double)height;
    double inum = 1.0 / num;
    double mean[3] = { sum[0] * inum, sum[1] * inum, sum[2] * inum };
    double sdev[3] = { sqrt(inum * sumsq[0] - mean[0] * mean[0]),
        sqrt(inum * sumsq[1] - mean[1] * mean[1]),
        sqrt(inum * sumsq[2] - mean[2] * mean[2]) };

    //  set mean-2*sdev to 16, and mean+2*sdev to 240 (but clamp to min/max values to avoid washing out)
    float minval[3] = { (float)std::max((double)min[0], mean[0] - 2 * sdev[0]),
        (float)std::max((double)min[1], mean[1] - 2 * sdev[1]),
        (float)std::max((double)min[2], mean[2] - 2 * sdev[2]) };
    float maxval[3] = { (float)std::min((double)max[0], mean[0] + 2 * sdev[0]),
        (float)std::min((double)max[1], mean[1] + 2 * sdev[1]),
        (float)std::min((double)max[2], mean[2] + 2 * sdev[2]) };
    float mul[3], add[3];
    for (int i = 0; i < 3; ++i) {
        mul[i] = (240 - 16) / (maxval[i] - minval[i]);
        add[i] = 16 - minval[i] * mul[i];
    }

    //  apply to each channel
    for (int row = 0, nrow = height; row != nrow; ++row) {
        unsigned char const *src = bits + rowbytes * row;
        unsigned char *dst = &oot[rowbytes * row];
        float dither[3] = { 0, 0, 0 };
        for (int col = 0, ncol = width; col != ncol; ++col) {
            for (int i = 0; i < 3; ++i) {
                float dv = src[i] * mul[i] + add[i] + dither[i];
                if (dv < 0) {
                    dv = 0;
                }
                else if (dv > 255) {
                    dv = 255;
                }
                dst[i] = floorf(dv + 0.5f);
                dither[i] = dv - dst[i];
            }
            src += Image::BytesPerPixel;
            dst += Image::BytesPerPixel;
        }
    }

    //  now make this the bits
    storage_.swap(oot);
    bits = &storage_[0];
}

template<typename T, typename C> void recursive_mark(int cluster, T &seed, C &spans) {
    assert(seed.cluster == -1 || seed.cluster == cluster);
    if (seed.cluster != -1) {
        return;
    }
    seed.cluster = cluster;
    for (size_t i = 0, n = seed.prev.size(); i != n; ++i) {
        recursive_mark(cluster, spans[seed.prev[i]], spans);
    }
    for (size_t i = 0, n = seed.next.size(); i != n; ++i) {
        recursive_mark(cluster, spans[seed.next[i]], spans);
    }
}

void Pixmap::find_areas_of_color(Area area, Color c, float tolerance, float normalization, int min_areas, std::vector<ColorArea> &o_areas) {

    struct startstop {
        startstop(int a, int o) :
            start(a), stop(o), height(0), cluster(-1) {
        }
        int start;
        int stop;
        int height;
        int cluster;
        std::vector<int> prev;
        std::vector<int> next;
    };

    //  build spans of accepted pixels across each row
    std::vector<startstop> spans;
    std::vector<size_t> rows;
    unsigned char max = c.maxcomponent();
    if (max == 0) {
        max = 1;
    }
    for (int y = std::max(0, area.top), ym = std::min(area.bottom(), height); y != ym; ++y) {
        startstop cur(0, -1);
        rows.push_back(spans.size());
        for (int x = std::max(0, area.left), xm = std::min(area.right(), width); x != xm; ++x) {
            Color cc(pixel(x, y));
            cc.scale(max, cc.maxcomponent(), normalization);
            if (cc.distance(c) < tolerance) {
                if (cur.stop == x) {
                    ++cur.stop;
                }
                else {
                    cur.start = x;
                    cur.stop = x + 1;
                }
            }
            else if (cur.stop > cur.start) {
                spans.push_back(cur);
                cur.stop = -1;
            }
        }
        if (cur.stop > cur.start) {
            spans.push_back(cur);
            cur.stop = -1;
        }
    }
    rows.push_back(spans.size());

    //  find overlapping spans
    for (size_t row = 1, nrows = rows.size()-1; row != nrows; ++row) {
        size_t prev = rows[row-1];
        size_t offset = rows[row];
        size_t end = rows[row+1];
        for (size_t i = offset; i != end; ++i) {
            startstop &ss(spans[i]);
            for (size_t p = prev; p != offset; ++p) {
                startstop &pss(spans[p]);
                if (pss.start >= ss.stop) {
                    break;
                }
                if (pss.stop > ss.start) {
                    if (pss.height + 1 > ss.height) {
                        ss.height = pss.height;
                    }
                    ss.prev.push_back(p);
                    pss.next.push_back(i);
                }
            }
        }
    }

    //  find connected clusters
    std::vector<size_t> clusterspans;
    for (size_t i = 0, n = spans.size(); i != n; ++i) {
        startstop &ss(spans[i]);
        if (ss.cluster != -1) {
            continue;
        }
        recursive_mark((int)clusterspans.size(), ss, spans);
        clusterspans.push_back(i);
    }

    //  now, I found clusters; calculate extent and COG of each
    std::vector<ColorArea> ret;
    ret.resize(clusterspans.size());
    struct center {
        center() : x(0), y(0), count(0) {}
        double x;
        double y;
        int count;
    };
    std::vector<center> centers;
    centers.resize(clusterspans.size());
    int row = area.top - 1;
    size_t rcnt = 0;    //  points at next row
    for (size_t i = 0, n = spans.size(); i != n; ++i) {
        while (rows[rcnt] == i) {
            ++rcnt;
            ++row;
            assert(rcnt < rows.size());
        }
        //  each span is in a cluster
        startstop &ss(spans[i]);
        assert(ss.cluster >= 0 && (size_t)ss.cluster < clusterspans.size());
        ColorArea &ca(ret[ss.cluster]);
        ca.color = c;
        ca.area.left = std::min(ca.area.left, ss.start);
        ca.area.top = std::min(ca.area.top, row);
        ca.area.width = std::max(ca.area.width, ss.stop-ca.area.left);
        ca.area.height = std::max(ca.area.height, row - ca.area.top + 1);
        center &ctr(centers[ss.cluster]);
        ctr.x += (ss.start + ss.stop - 1) * 0.5 * (ss.stop - ss.start);
        ctr.y += row * (ss.stop - ss.start);
        ctr.count += (ss.stop - ss.start);
    }

    //  calculate centers of gravity and pecent fill
    for (size_t i = 0, n = ret.size(); i != n; ++i) {
        ColorArea &ca(ret[i]);
        center &ctr(centers[i]);
        ca.cog.left = (int)floor(ctr.x / ctr.count);
        ca.cog.top = (int)floor(ctr.y / ctr.count);
        ca.weight = (float)((double)ctr.count / ca.area.area());
    }

    //  add qualifying clusters to output
    for (size_t i = 0, n = ret.size(); i != n; ++i) {
        ColorArea &ca(ret[i]);
        if ((int)floor(ca.area.area() * ca.weight) >= min_areas) {
            o_areas.push_back(ca);
        }
    }
}

size_t Pixmap::get_tga_header(void *data, size_t size) {
    unsigned char hdr[18] = { 0 };
    hdr[2] = 2;     //  RGB
    hdr[7] = 8 * Image::BytesPerPixel;
    hdr[12] = width & 0xff;
    hdr[13] = (width >> 8) & 0xff;
    hdr[14] = height & 0xff;
    hdr[15] = (height >> 8) & 0xff;
    hdr[16] = 8 * Image::BytesPerPixel;
    hdr[17] = 0x20; //  vertical flip
    memcpy(data, hdr, std::min(size, sizeof(hdr)));
    return sizeof(hdr);
}

void Pixmap::to_tga_order() {
    if (!tgaorder_) {
        std::vector<unsigned char> oot;
        oot.resize(rowbytes * height);
        tgaorder_ = true;
        for (int row = 0; row != height; ++row) {
            unsigned char const *ptr = bits + row * rowbytes;
            unsigned char *dst = &oot[row * rowbytes];
            for (int col = 0, ncol = width; col != ncol; ++col) {
                dst[0] = ptr[2];
                dst[1] = ptr[1];
                dst[2] = ptr[0];
                if (Image::BytesPerPixel > 3) {
                    dst[3] = ptr[3];
                }
                ptr += Image::BytesPerPixel;
                dst += Image::BytesPerPixel;
            }
        }
        storage_.swap(oot);
        bits = &storage_[0];
    }
}

void Pixmap::frame_rect(Area const &a, Color c) {
    Area clip(a);
    unsigned char *base = write_bits() + rowbytes * a.top + Image::BytesPerPixel * a.left;
    unsigned char *top = base;
    unsigned char *bottom = base + rowbytes * (a.height - 1);
    for (int x = a.left, n = a.right(); x != n; ++x) {
        if (x >= 0 && x < width) {
            if (a.top >= 0 && a.top < height) {
                top[0] = c.r;
                top[1] = c.g;
                top[2] = c.b;
            }
            if (a.bottom() >= 0 && a.bottom() <= height) {
                bottom[0] = c.r;
                bottom[1] = c.g;
                bottom[2] = c.b;
            }
        }
        top += Image::BytesPerPixel;
        bottom += Image::BytesPerPixel;
    }
    unsigned char *left = base;
    unsigned char *right = base + Image::BytesPerPixel * (a.width - 1);
    for (int y = a.top, n = a.bottom(); y != n; ++y) {
        if (y >= 0 && y < height) {
            if (a.left >= 0 && a.left < width) {
                left[0] = c.r;
                left[1] = c.g;
                left[2] = c.b;
            }
            if (a.right() >= 0 && a.right() <= width) {
                right[0] = c.r;
                right[1] = c.g;
                right[2] = c.b;
            }
        }
        left += rowbytes;
        right += rowbytes;
    }
}

unsigned char *Pixmap::write_bits() {
    if (storage_.size() == 0 || bits != &storage_[0]) {
        storage_.resize(height * rowbytes);
        memcpy(&storage_[0], bits, height * rowbytes);
        bits = &storage_[0];
    }
    return &storage_[0];
}

