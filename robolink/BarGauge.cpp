#include "BarGauge.h"
#include <FL/fl_draw.H>
#include <iostream>


BarGauge::BarGauge(int x, int y, int w, int h) :
    Fl_Widget(x, y, w, h),
    min_(0),
    max_(100),
    fg_(fl_rgb_color(0x30, 0xc0, 0x30)),
    bg_(fl_rgb_color(0xc0, 0xc0, 0x30))
{
}

void BarGauge::draw()
{
    int wid = w();
    int hei = h();
    int v = value_;
    if (v < min_)
        v = min_;
    else if (v > max_)
        v = max_;
    int bar = wid * (v - min_) / (max_ - min_);
    if (bar > 0)
    {
        fl_color(fg_);
        fl_rectf(x()+0, y()+0, bar, hei);
    }
    if (bar < wid)
    {
        fl_color(bg_);
        fl_rectf(x()+bar, y()+0, wid-bar, hei);
    }
}

void BarGauge::value(int v)
{
    if (v != value_)
    {
        value_ = v;
        redraw();
    }
}

