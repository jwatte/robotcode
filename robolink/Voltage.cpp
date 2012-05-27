
#include "Voltage.h"

#include <stdio.h>

#include <FL/Fl.H>

#define VOLTAGE_THRESHOLD 11.5f

void shutdown(void *)
{
    system("killall lxsession");
}


void Voltage::on_voltage(float v)
{
    sprintf(volts_, "%.1f V", v);
    text_->label(volts_);
    dial_->value(v);
    if (v <= VOLTAGE_THRESHOLD)
    {
        if (!timer_)
        {
            Fl::add_timeout(60, shutdown);
            text_->label("DEAD!");
            text_->labelcolor(0xff000000);
            dial_->color(0xff000000);
            dial_->color2(0xffffff00);
        }
    }
}

void Voltage::make_widgets()
{
    group_ = new Fl_Group(0, 0, 200, 200);
    dial_ = new Fl_Dial(25, 25, 150, 150);
    dial_->minimum(10.0f);
    dial_->maximum(16.0f);
    dial_->value(13.0f);
    dial_->type(FL_LINE_DIAL);
    dial_->color(FL_GREEN);
    dial_->color2(FL_RED);
    text_ = new Fl_Box(150, 175, 50, 25, "13.0 V");

}

