
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <jpeglib.h>
#include <vector>
#include <stdexcept>

#include "ay.h"


ImagePtr Image::create(size_t width, size_t height, size_t rowbytes, void *data) {
    if (rowbytes == 0) {
        rowbytes = width * 3;
    }
    return ImagePtr(new Image(rowbytes*height, width, height, rowbytes, (unsigned char*)data));
}

Image::Image(size_t sz, size_t w, size_t h, size_t rb, void *dt) :
    data_(dt ? (unsigned char *)dt : new unsigned char[sz]),
    size_(sz),
    rowbytes_(rb),
    width_(w),
    height_(h),
    owned_(dt == 0)
{
}

Image::~Image() {
    if (owned_) {
        delete[] data_;
    }
}

ImagePtr load_jpg(std::string const &path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Could not open JPG: " + path);
    }
    size_t sz = lseek(fd, 0, 2);
    if (sz < 100 || sz > 0x10000000) {
        close(fd);
        throw std::runtime_error("Bad JPG file size: " + path);
    }
    lseek(fd, 0, 0);
    std::vector<unsigned char> data;
    data.resize(sz);
    if (sz != read(fd, &data[0], sz)) {
        close(fd);
        throw std::runtime_error("Error reading JPG: " + path);
    }
    close(fd);

    struct jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(cinfo));
    struct jpeg_error_mgr jerr;
    memset(&jerr, 0, sizeof(jerr));
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, &data[0], sz);
    jpeg_read_header(&cinfo, true);
    jpeg_start_decompress(&cinfo);
    size_t rowbytes = cinfo.output_width * 3;

    ImagePtr img = Image::create(cinfo.output_width, cinfo.output_height);
    std::vector<unsigned char *> rows;
    rows.resize(img->height());
    for (size_t i = 0, n = img->height(); i != n; ++i) {
        rows[i] = img->data() + img->rowbytes() * i;
    }
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &rows[cinfo.output_scanline], 
                cinfo.output_height - cinfo.output_scanline);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return img;
}

void save_jpg(ImagePtr img, std::string const &path) {
    struct jpeg_compress_struct cinfo;
    memset(&cinfo, 0, sizeof(cinfo));
    struct jpeg_error_mgr jerr;
    memset(&jerr, 0, sizeof(jerr));
    std::string tmp(path + ".tmp");
    FILE *ofile = fopen(tmp.c_str(), "wb");
    if (!ofile) {
        throw std::runtime_error("Could not create JPG: " + tmp);
    }

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, ofile);
    cinfo.image_width = img->width();
    cinfo.image_height = img->height();
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 50, true);
    jpeg_start_compress(&cinfo, true);

    std::vector<unsigned char *> rows;
    rows.resize(img->height());
    for (size_t i = 0, n = img->height(); i != n; ++i) {
        rows[i] = img->data() + img->rowbytes() * i;
    }
    while (cinfo.next_scanline < cinfo.image_height) {
        jpeg_write_scanlines(&cinfo, &rows[cinfo.next_scanline],
                cinfo.image_height - cinfo.next_scanline);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(ofile);

    if (rename(tmp.c_str(), path.c_str()) < 0) {
        throw std::runtime_error("Could not replace file: " + path);
    }
}

ImagePtr load_tga(std::string const &path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        throw std::runtime_error("Could not open image: " + path);
    }
    unsigned short hdr[9];
    if (18 != read(fd, hdr, 18)) {
        close(fd);
        throw std::runtime_error("TGA header read error: " + path);
    }
    if (hdr != 0 || hdr[1] != 2 || (hdr[8] & 0xff) != 24) {
        close(fd);
        throw std::runtime_error("TGA not 24-bit: " + path);
    }
    ImagePtr img(Image::create(hdr[6], hdr[7]));
    if (img->size() != read(fd, img->data(), img->size())) {
        close(fd);
        throw std::runtime_error("TGA read error: " + path);
    }
    if (!(hdr[8] & 0x2000)) {
        flip_v_and_rb(img);
    }
    else {
        flip_rb(img);
    }
    close(fd);
    return img;
}

void save_tga(ImagePtr img, std::string const &path) {
    std::string tmp(path + ".tmp");
    int fd = open(tmp.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0664);
    if (fd < 0) {
        throw std::runtime_error("Could not create file: " + tmp);
    }
    unsigned short hdr[9] = { 0, 0x2, 0, 0x1800, 0, 0, img->width(), img->height(), 0x2018 };
    if (18 != write(fd, hdr, 18)) {
        unlink(tmp.c_str());
        close(fd);
        throw std::runtime_error("Could not write header: " + tmp);
    }
    flip_rb(img);
    if (img->size() != write(fd, img->data(), img->size())) {
        unlink(tmp.c_str());
        close(fd);
        flip_rb(img);   //  put it back
        throw std::runtime_error("Could not write data: " + tmp);
    }
    flip_rb(img);   //  put it back
    if (rename(tmp.c_str(), path.c_str()) < 0) {
        throw std::runtime_error("Could not replace file: " + path);
    }
}

ImagePtr load_image(std::string const &path) {
    if (path.find(".jpg") != std::string::npos) {
        return load_jpg(path);
    }
    else if (path.find(".tga") != std::string::npos) {
        return load_tga(path);
    }
    else {
        throw std::runtime_error("Unknown image format in load: " + path);
    }
}

void save_image(ImagePtr image, std::string const &path) {
    if (path.find(".jpg") != std::string::npos) {
        save_jpg(image, path);
    }
    else if (path.find(".tga") != std::string::npos) {
        save_tga(image, path);
    }
    else {
        throw std::runtime_error("Unknown image format in save: " + path);
    }
}

void flip_v(ImagePtr image) {
    unsigned char *ptr = image->data();
    size_t rb = image->rowbytes();
    size_t h = image->height();
    unsigned char *top = ptr + rb * (h - 1);
    std::vector<unsigned char> scanline;
    scanline.resize(rb);
    while (ptr < top) {
        memcpy(&scanline[0], ptr, rb);
        memcpy(ptr, top, rb);
        memcpy(top, &scanline[0], rb);
        ptr += rb;
        top -= rb;
    }
}

void flip_rb(ImagePtr image) {
    unsigned char *ptr = image->data();
    size_t rb = image->rowbytes();
    size_t h = image->height();
    size_t n = h * rb / 3;
    for (size_t i = 0; i != n; ++i) {
        unsigned char ch = ptr[0];
        ptr[0] = ptr[2];
        ptr[2] = ch;
        ptr += 3;
    }
}

void flip_v_and_rb(ImagePtr image) {
    //  TODO: I could do this much more efficiently in one pass...
    flip_v(image);
    flip_rb(image);
}


