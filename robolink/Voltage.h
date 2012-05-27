#if !defined(Voltage_h)
#define Voltage_h

#include <FL/Fl_Dial.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>

class Voltage
{
public:
    void on_voltage(float v);
    void make_widgets();

    Fl_Group *group_;
    Fl_Dial *dial_;
    Fl_Box *text_;
    char volts_[8];
    bool timer_;
};

extern Voltage voltage;

#endif
