#include "Image.h"
#include <stdexcept>
#include <cstring>
#include <iostream>
#include <jpeglib.h>
#include <assert.h>


//  comes from hdr.cpp
extern unsigned char huff_table[];
extern unsigned int huff_size;


Image::Image() :
    width_(0),
    height_(0),
    dirty_(0) {
}

Image::~Image() {
}

void *Image::alloc_compressed(size_t size) {
    compressed_.resize(size + huff_size);
    return &compressed_[huff_size];
}

void Image::complete_compressed(size_t size) {
    unsigned char *ptr = (unsigned char *)&compressed_[huff_size];
    unsigned char *end = ptr + size;
    while (ptr < end) {
        if (ptr[0] == 0xff && ptr[1] == 0xda) {
            break;
        }
        ++ptr;
    }
    if (ptr == end) {
        throw std::runtime_error("Invalid MJPEG data in Image::complete_compressed()");
    }
    unsigned char *dstart = (unsigned char *)&compressed_[huff_size];
    memmove(&compressed_[0], dstart, ptr - dstart);
    memcpy(ptr - huff_size, huff_table, huff_size);
    dirty_ = size;
}

size_t Image::width() const {
    undirty();
    return width_;
}

size_t Image::height() const {
    undirty();
    return height_;
}

size_t Image::width_t() const {
    undirty();
    return width_ >> 2;
}

size_t Image::height_t() const {
    undirty();
    return height_ >> 2;
}

void const *Image::bits(ImageBits ib) const {
    undirty();
    return &vec(ib)[0];
}

size_t Image::size(ImageBits ib) const {
    undirty();
    return vec(ib).size();
}

std::vector<char> const &Image::vec(ImageBits ib) const {
    switch (ib) {
        case CompressedBits:
            return compressed_;
        case FullBits:
            undirty();
            return uncompressed_;
        case ThumbnailBits:
            undirty();
            return thumbnail_;
        default:
            throw std::runtime_error("Unknown ImageBits in Image");
    }
}

void Image::undirty() const {
    if (dirty_) {
        decompress(dirty_ + huff_size);
        make_thumbnail();
        dirty_ = 0;
    }
}


void Image::decompress(size_t size) const {
    jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(cinfo));
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (unsigned char *)&compressed_[0], size);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    width_ = cinfo.output_width;
    height_ = cinfo.output_height;
    uncompressed_.resize(width_ * height_ * BytesPerPixel);
    //  this is so ghetto!
    JSAMPLE *ary[8];
    unsigned char *data = (unsigned char *)&uncompressed_[0];
    while (cinfo.output_scanline < cinfo.output_height)
    {
        for (unsigned int i = 0; i < 8; ++i)
        {
            ary[i] = (unsigned char *)data + (cinfo.output_scanline + i) *
                cinfo.output_width * BytesPerPixel;
        }
        jpeg_read_scanlines(&cinfo, ary, 8);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

void Image::make_thumbnail() const {
    thumbnail_.resize(uncompressed_.size() >> 4);
    unsigned char *base = (unsigned char *)&uncompressed_[0];
    size_t rowbytes = BytesPerPixel * width_;
    size_t rowbytes2 = rowbytes * 2;
    size_t rowbytes3 = rowbytes * 3;
    size_t colcnt = width_ >> 2;
    size_t rb = BytesPerPixel - 2;
    unsigned char *tnp = (unsigned char *)&thumbnail_[0];
    for (size_t col = 0, i = 0, n = thumbnail_.size(); i != n; i += BytesPerPixel) {
        unsigned char *p = base;
        unsigned short red = 8, green = 8, blue = 8;
        //  If we're unlucky with aliasing, this needs a 5x L1 cache.
        //  For size 1920, though, it'll probably not alias that badly.
        for (size_t q = 0; q != 4; ++q) {
            red += p[0] + p[rowbytes] + p[rowbytes2] + p[rowbytes3];
            p++;
            green += p[0] + p[rowbytes] + p[rowbytes2] + p[rowbytes3];
            p++;
            blue += p[0] + p[rowbytes] + p[rowbytes2] + p[rowbytes3];
            p += rb;
        }
        tnp[i] = red >> 4;
        tnp[i+1] = green >> 4;
        tnp[i+2] = blue >> 4;
        col += 1;
        base += BytesPerPixel << 2;
        if (col == colcnt) {
          //  This assumes rowbytes is tightly packed
            base += rowbytes3;
            col = 0;
        }
    }
}


