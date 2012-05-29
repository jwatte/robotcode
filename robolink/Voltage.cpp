
#include "Voltage.h"

#include <stdio.h>

#include <FL/Fl.H>

#define VOLTAGE_THRESHOLD 11.5f
#define MOTOR_VOLTAGE_THRESHOLD 6.6f

void shutdown(void *)
{
    system("killall lxsession");
}


void Voltage::on_main_voltage(float v)
{
    sprintf(voltsMain_, "%.1f V", v);
    textMainPower_->label(voltsMain_);
    dialMainPower_->value(v);
    if (v <= VOLTAGE_THRESHOLD)
    {
        if (!timer_)
        {
            Fl::add_timeout(60, shutdown);
            textMainPower_->label("DEAD!");
            textMainPower_->labelcolor(0xff000000);
            dialMainPower_->color(0xff000000);
            dialMainPower_->color2(0xffffff00);
        }
    }
}

void Voltage::on_motor_voltage(float v)
{
    sprintf(voltsMotor_, "%.1f V", v);
    textMotorPower_->label(voltsMotor_);
    dialMotorPower_->value(v);
    if (v <= MOTOR_VOLTAGE_THRESHOLD)
    {
        textMotorPower_->label("DEAD!");
        textMotorPower_->labelcolor(0xff000000);
        dialMotorPower_->color(0xffff0000);
        dialMotorPower_->color2(0x00ffff00);
    }
}

void Voltage::make_widgets()
{
    group_ = new Fl_Group(0, 0, 200, 200);

    dialMainPower_ = new Fl_Dial(10, 10, 85, 85);
    dialMainPower_->minimum(10.0f);
    dialMainPower_->maximum(16.0f);
    dialMainPower_->value(13.0f);
    dialMainPower_->type(FL_LINE_DIAL);
    dialMainPower_->color(FL_GREEN);
    dialMainPower_->color2(FL_RED);
    textMainPower_ = new Fl_Box(25, 100, 50, 25, "13.0 V");

    dialMotorPower_ = new Fl_Dial(105, 10, 85, 85);
    dialMotorPower_->minimum(6.3f);
    dialMotorPower_->maximum(8.5f);
    dialMotorPower_->value(7.4f);
    dialMotorPower_->type(FL_LINE_DIAL);
    dialMotorPower_->color(FL_BLUE);
    dialMotorPower_->color2(FL_YELLOW);
    textMotorPower_ = new Fl_Box(125, 100, 50, 25, "7.5 V");

    group_->end();
}

