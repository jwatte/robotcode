
#if !defined(rl2_GraphWidget_h)
#define rl2_GraphWidget_h

#include <FL/Fl_Widget.H>
#include <time.h>
#include <vector>

class GraphWidget : public Fl_Widget {
public:
    GraphWidget(int x, int y, int w = 140, int h = 40, char const *l = 0);
    ~GraphWidget();
    void range(double low, double high);
    void granule(long l);
    void clear();
    void scroll();
    virtual void value(double d);
    virtual void draw();
    virtual void resize(int x, int y, int w, int h);
private:
    std::vector<std::pair<double, double>> vals_;
    time_t left_;
    time_t granule_;
    double low_;
    double high_;
    double last_;
};

#endif  //  rl2_GraphWidget_h
