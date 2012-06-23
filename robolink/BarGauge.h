#if !defined(BarGauge_h)
#define BarGauge_h

#include <FL/Fl_Widget.H>

class BarGauge : public Fl_Widget
{
public:
    BarGauge(int x, int y, int w, int h);
    void draw();
    void value(int v);

    int value_;
    int min_;
    int max_;
    Fl_Color fg_;
    Fl_Color bg_;
};

#endif  //  BarGauge_h
