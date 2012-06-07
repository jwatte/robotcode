
#include "Voltage.h"
#include "Board.h"

#include <stdio.h>

#include <FL/Fl.H>


Voltage::Voltage() :
    group_(0),
    mainVoltage_(11.0f, 16.0f),
    motorVoltage_(6.4f, 8.5f)
{
}

void Voltage::init(MotorPowerBoard *mpb, UsbLinkBoard *ulb)
{
    group_ = new Fl_Group(0, 0, 200, 200);
    mainVoltage_.init(10, 10, &ulb->voltage_);
    motorVoltage_.init(105, 10, &mpb->voltage_);
    group_->end();
}


VoltageDial::VoltageDial(float min, float max) :
    src_(0),
    min_(min),
    max_(max),
    dial_(0),
    text_(0)
{
    memset(labelStr_, 0, sizeof(labelStr_));
}

void VoltageDial::init(int x, int y, Value<unsigned char> *src)
{
    src_ = src;
    dial_ = new Fl_Dial(x, y, 85, 85);
    dial_->minimum(min_);
    dial_->maximum(max_);
    dial_->value(min_);
    dial_->type(FL_LINE_DIAL);
    dial_->color(FL_BLUE);
    dial_->color2(FL_YELLOW);
    text_ = new Fl_Box(x + 20, y + 90, 50, 25, "");
    src_->add_listener(this);
    invalidate();
}

void VoltageDial::invalidate()
{
    float val = src_->value() * (1.0f / 16.0f);
    if (val < min_) {
        val = min_;
    }
    if (val > max_) {
        val = max_;
    }
    sprintf(labelStr_, "%.1f V", val);
    text_->label(labelStr_);
    if (val < min_ + (max_ - min_) * 0.2f) {
        text_->color(FL_RED);
    }
    else {
        text_->color(FL_GREEN);
    }
    dial_->value(val);
}

