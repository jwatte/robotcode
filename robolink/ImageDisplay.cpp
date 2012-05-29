#include "ImageDisplay.h"

#include <FL/Fl.H>
#include <FL/fl_draw.H>


extern unsigned char huff_table[];
extern unsigned int huff_size;




Fl_ImageBox::Fl_ImageBox(ImageDisplay *id, int x, int y, int w, int h) :
    Fl_Widget(x, y, w, h)
{
    id_ = id;
}

void Fl_ImageBox::draw()
{
#if 0
    unsigned char ch[18] = {
        0, 0, 2,
        0, 0, 0, 0, 0,
        0, 0, 0, 0, 
        id_->width_ & 0xff, id_->width_ >> 8,
        id_->height_ & 0xff, id_->height_ >> 8,
        24, 0 };
    FILE *f = fopen("file.tga", "wb");
    fwrite(ch, 18, 1, f);
    fwrite(id_->data_, id_->width_ * id_->height_ * 3, 1, f);
    fclose(f);
#endif
    fl_draw_image((unsigned char const *)id_->qData_, x(), y(), id_->width_ >> 2, id_->height_ >> 2, 3, 0);
}






ImageDisplay::ImageDisplay(int x, int y, int w, int h)
{
    box_ = new Fl_ImageBox(this, x, y, w, h);
    src_ = 0;
    data_ = 0;
    width_ = 0;
    height_ = 0;
}

ImageDisplay::~ImageDisplay()
{
    delete box_;
    free(data_);
}

int n = 0;

void ImageDisplay::invalidate()
{
    unsigned char *dst = (unsigned char *)src_->get_jpg();
    unsigned char *ptr = dst;
    size_t size = src_->get_size();
    size_t padding = src_->get_padding();
    if (padding < huff_size)
    {
        fprintf(stderr, "Too little padding! need %llu, got %llu\n", 
            (unsigned long long)huff_size, (unsigned long long)padding);
        //  can't do this!
        return;
    }

    //  find the spot to put the header
    while (ptr < dst + size)
    {
        if (ptr[0] == 0xff && ptr[1] == 0xda)
        {
            break;
        }
        ++ptr;
    }
    if (ptr == dst + size)
    {
        fprintf(stderr, "bad input MJPEG format\n");
        return;
    }

    //  splice the huffman table in front of the data
    memmove(dst - huff_size, dst, ptr - dst);
    memcpy(ptr - huff_size, huff_table, huff_size);

#if 0
    char buf[100];
    sprintf(buf, "image%04i.jpg", n);
    ++n;
    FILE *f = fopen(buf, "wb");
    fwrite(dst-huff_size, size+huff_size, 1, f);
    fclose(f);
#endif

    jpeg_decompress_struct cinfo;
    memset(&cinfo, 0, sizeof(cinfo));
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, (unsigned char *)dst - huff_size, size + huff_size);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    width_ = cinfo.output_width;
    height_ = cinfo.output_height;
    data_ = realloc(data_, cinfo.output_width * cinfo.output_height * 3);
    qData_ = realloc(qData_, (cinfo.output_width >> 2) * (cinfo.output_height >> 2) * 3);
    //  this is so ghetto!
    JSAMPLE *ary[8];
    unsigned char *oq = (unsigned char *)qData_;
    int n = 0;
    while (cinfo.output_scanline < cinfo.output_height)
    {
        for (unsigned int i = 0; i < 8; ++i)
        {
            ary[i] = (unsigned char *)data_ + (cinfo.output_scanline + i) *
                cinfo.output_width * 3;
        }
        int dim = jpeg_read_scanlines(&cinfo, ary, 8);
        for (int j = 0; j < dim; ++j)
        {
            ++n;
            if (n == 4)
            {
                n = 0;
                unsigned char *src = ary[j];
                for (int i = 0; i < cinfo.output_width; i += 4)
                {
                    *oq++ = src[0];
                    *oq++ = src[1];
                    *oq++ = src[2];
                    src += 12;
                }
            }
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    box_->redraw();
}

void ImageDisplay::set_source(VideoCapture *src)
{
    if (src_ != 0)
    {
        src_->remove_listener(this);
    }
    src_ = src;
    if (src)
    {
        src->add_listener(this);
    }
}

