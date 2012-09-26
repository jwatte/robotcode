
#include "GraphWidget.h"
#include <FL/fl_draw.H>
#include <iostream>
#include <cmath>


GraphWidget::GraphWidget(int x, int y, int w, int h, char const *l) :
    Fl_Widget(x, y, w, h, l),
    granule_(10),
    low_(0),
    high_(100),
    last_(50) {
    color(0xffffffff);
    time(&left_);
}

GraphWidget::~GraphWidget() {
}

void GraphWidget::range(double low, double high) {
    low_ = low;
    high_ = high;
    if (low_ == high_) {
        high_ = low_ + 1;
    }
    last_ = (low_ + high_) * 0.5;
    damage(0xff);
}

void GraphWidget::granule(long l) {
    granule_ = std::max(1L, l);
}

void GraphWidget::scroll() {
    if (!vals_.empty()) {
        value(last_);
    }
}

void GraphWidget::value(double d) {
    last_ = d;
    time_t now;
    time(&now);
    int dx = (now - left_) / granule_;
    if (dx >= w()) {
        left_ += granule_ * (1 + dx - w());
        dx = w() - 1;
        vals_.erase(vals_.begin());
    }
    if (vals_.size() <= (size_t)dx) {
        if (vals_.empty()) {
            vals_.push_back(std::pair<double, double>(d, d));
            left_ = now;
        }
        else {
            vals_.resize(dx + 1, *vals_.rbegin());
            vals_[dx] = std::pair<double, double>(d, d);
        }
    }
    else {
        vals_[dx].first = std::min(vals_[dx].first, d);
        vals_[dx].second = std::max(vals_[dx].second, d);
    }
    damage(0xff);
}

void GraphWidget::draw() {
    //  background
    fl_rectf(x(), y(), w(), h(), color());

    //  horizontal lines
    fl_color(0xc0c0c0ff);
    //  halves just happen to work for now -- for other graphs, 
    //  probably want a different gradation
    double l = ceil(low_ * 2);
    double s = h() / (low_ - high_) * 0.5;
    double t = y() + h() - low_ * s * 2;
    double x2 = x() + w();
    while (l < high_ * 2) {
        double yl = t + s * l;
        fl_line(x(), yl, x2, yl);
        l += 1;
    }

    //  vertical lines
    fl_color(0xc0c0c0ff);
    //  12 is a nice multiplier for time-based gradations;
    //  1, 5, 10 seconds all work well with it
    long tl = left_ - (left_ % 12 * granule_);
    long rl = left_ + w() * granule_;
    s = y();
    t = y() + h();
    while (tl < rl) {
        if (tl >= left_) {
            double x2 = x() + (tl - left_) / granule_;
            fl_line(x2, s, x2, t);
        }
        tl += 12 * granule_;
    }

    //  text
    fl_font(FL_HELVETICA, 9);
    fl_color(0x20a040ff);
    char str[20];
    sprintf(str, "%+.1f", high_);
    fl_draw(str, x(), y()+9);
    sprintf(str, "%+.1f", low_);
    fl_color(0x802020ff);
    fl_draw(str, x(), y()+h());

    // graph
    fl_color(FL_FOREGROUND_COLOR);
    if (vals_.size() > 0) {
        int dx = x();
        int dy = y() + h() - 1;
        for (size_t i = 0, n = vals_.size(); i != n; ++i) {
            double v0 = std::min(high_, std::max(low_, vals_[i].first));
            double v1 = std::min(high_, std::max(low_, vals_[i].second));
            int p = (int)((h() - 1) * (v0 - low_) / (high_ - low_));
            int q = (int)((h() - 1) * (v1 - low_) / (high_ - low_));
            fl_line(dx + i, dy - p, dx + i, dy - q);
        }
    }
    draw_label();
}

void GraphWidget::resize(int x, int y, int w, int h) {
    Fl_Widget::resize(x, y, w, h);
    if ((size_t)h < vals_.size()) {
        vals_.erase(vals_.begin(), vals_.begin() + vals_.size() - h);
    }
    damage(0xff);
}
