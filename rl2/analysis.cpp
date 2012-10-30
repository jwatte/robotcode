
#include "analysis.h"
#include <string.h>



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

void Pixmap::find_areas_of_color(Color c, float tolerance, float normalization, int min_areas, std::vector<ColorArea> &o_areas) {

    struct startstop {
        startstop(int a, int o) :
            start(a), stop(o) {
        }
        int start;
        int stop;
    };

    //  build spans of accepted pixels across each row
    std::vector<startstop> spans;
    std::vector<size_t> rows;
    unsigned char max = c.maxcomponent();
    if (max == 0) {
        max = 1;
    }
    for (int y = 0, ym = height; y != ym; ++y) {
        startstop cur(0, -1);
        rows.push_back(spans.size());
        for (int x = 0, xm = width; x != xm; ++x) {
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
