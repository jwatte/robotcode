#if !defined(Voltage_h)
#define Voltage_h

#include <FL/Fl_Dial.H>
#include <FL/Fl_Group.H>
#include <FL/Fl_Box.H>

#include "Talker.h"
#include "Value.h"

class MotorPowerBoard;
class UsbLinkBoard;

class VoltageDial : public Listener
{
public:
    VoltageDial(float min, float max);
    void init(int x, int y, Value<unsigned char> *src);
    void invalidate();

    Value<unsigned char> *src_;
    float min_;
    float max_;
    char labelStr_[8];
    Fl_Dial *dial_;
    Fl_Box *text_;
};

class Voltage
{
public:
    Voltage();
    void init(MotorPowerBoard *mpb, UsbLinkBoard *ulb);

    Fl_Group *group_;
    VoltageDial mainVoltage_;
    VoltageDial motorVoltage_;
};

extern Voltage voltage;

#endif
