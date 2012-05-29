#if !defined(ImageDisplay_h)
#define ImageDisplay_h

#include "Talker.h"
#include "VideoCapture.h"
#include <jpeglib.h>
#include <FL/Fl_Widget.H>


class ImageDisplay;

class Fl_ImageBox : public Fl_Widget {
public:
    Fl_ImageBox(ImageDisplay *id, int x, int y, int w, int h);
    void draw();

    ImageDisplay *id_;
};

class ImageDisplay : public Listener {
public:
    ImageDisplay(int x = 0, int y = 0, int w = 1280, int h = 720);
    ~ImageDisplay();
    void invalidate();
    void set_source(VideoCapture *src);

    Fl_ImageBox *box_;
    VideoCapture *src_;
    void *data_;
    size_t width_;
    size_t height_;
    void *qData_;
};

#endif  //  ImageDisplay_h

