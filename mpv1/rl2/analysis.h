
#if !defined(rl2_analysis_h)
#define rl2_analysis_h

#include "Image.h"
#include <vector>
#include <climits>
#include <boost/shared_ptr.hpp>

typedef boost::shared_ptr<Image> ImagePtr;

struct Color {
    Color(unsigned char ir, unsigned char ig, unsigned char ib) :
        r(ir), g(ig), b(ib) {
    }
    unsigned char r, g, b;
    //  normalized from 0 to 1
    float distance(Color const &o) const {
        float dr = (float)r - (float)o.r;
        float dg = (float)g - (float)o.g;
        float db = (float)b - (float)o.b;
        return ((float)dr*dr + (float)dg*dg + (float)db*db) * (1.0f / (3 * 255.0f * 255.0f));
    }
    unsigned char maxcomponent() const {
        if (r > g) {
            if (r > b) {
                return r;
            }
            return b;
        }
        if (g > b) {
            return g;
        }
        return b;
    }
    void scale(unsigned char numer, unsigned char denom, float amount) {
        if (!denom) {
            return;
        }
        float s = (float)numer / (float)denom * amount + (1.0f - amount);
        int nr = r * s + 0.5;
        int ng = g * s + 0.5;
        int nb = b * s + 0.5;
        r = std::max(0, std::min(255, nr));
        g = std::max(0, std::min(255, ng));
        b = std::max(0, std::min(255, nb));
    }
    Color complement() const {
        int rr = r;
        int gg = g;
        int bb = b;
        if (rr < 64 || rr >= 192) {
            rr = 255 - rr;
        }
        else {
            rr = rr ^ 0x80;
        }
        if (gg < 64 || gg >= 192) {
            gg = 255 - gg;
        }
        else {
            gg = gg ^ 0x80;
        }
        if (bb < 64 || bb >= 192) {
            bb = 255 - bb;
        }
        else {
            bb = bb ^ 0x80;
        }
        return Color(rr, gg, bb);
    }
};

struct Coordinate {
    Coordinate(int l, int t) :
        left(l), top(t) {
    }
    int left;
    int top;
};

struct Area {
    Area(int l, int t, int w, int h) :
        left(l), top(t), width(w), height(h) {
    }
    int left;
    int top;
    int width;
    int height;
    int right() const { return left + width; }
    int bottom() const { return top + height; }
    double area() const { return std::max((double)width, 0.0) * std::max((double)height, 0.0); }
};

struct ColorArea {
    ColorArea() :
        color(0, 0, 0),
        area(INT_MAX, INT_MAX, INT_MIN, INT_MIN),
        cog(INT_MIN, INT_MIN),
        weight(0) {
    }
    Color color;
    Area area;
    Coordinate cog; //  center of gravity
    float weight;   //  highest % of pixels in Area matching
};

class Pixmap {
public:
    Pixmap(boost::shared_ptr<Image> const &img, bool thumbnail) {
        ImageBits kind = (thumbnail ? ThumbnailBits : FullBits);
        bits = (unsigned char const *)img->bits(kind);
        width = img->width(kind);
        height = img->height(kind);
        rowbytes = img->size(kind) / height;
        tgaorder_ = false;
    }
    Color pixel(int l, int t) const {
        if (l < 0 || l >= width || t < 0 || t >= height) {
            return Color(0, 0, 0);
        }
        unsigned char const *pp = bits + rowbytes * t + Image::BytesPerPixel * l;
        return *(Color const *)pp;
    }

    void frame_rect(Area const &a, Color c);

    void color_correct();
    void find_areas_of_color(Area a, Color c, float tolerance, float normalization, int min_areas, std::vector<ColorArea> &o_areas);
    size_t get_tga_header(void *data, size_t buf);
    void to_tga_order();
    unsigned char *write_bits();

    unsigned char const *bits;
    size_t rowbytes;
    int width;
    int height;
    std::vector<unsigned char> storage_;
    bool tgaorder_;
};


#endif  //  rl2_analysis_h

