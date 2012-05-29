#if !defined(Voltage_h)
#define Voltage_h

#include <FL/Fl_Dial.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>

class Voltage
{
public:
    void on_main_voltage(float v);
    void on_motor_voltage(float v);
    void make_widgets();

    Fl_Group *group_;
    Fl_Dial *dialMainPower_;
    Fl_Box *textMainPower_;
    Fl_Dial *dialMotorPower_;
    Fl_Box *textMotorPower_;
    char voltsMain_[8];
    char voltsMotor_[8];
    bool timer_;
};

extern Voltage voltage;

#endif
