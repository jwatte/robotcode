#if !defined(Decisions_h)
#define Decisions_h

#include "Talker.h"
#include "Value.h"

class MotorPowerBoard;
class UsbLinkBoard;
class SensorBoard;
class Board;
class VideoCapture;
class UsbComm;

class Decisions : public Listener
{
public:
    Decisions(
        MotorPowerBoard *mpb,
        Board *estop,
        SensorBoard *sens,
        UsbLinkBoard *ulb,
        VideoCapture *vc0,
        VideoCapture *vc1,
        Talker *t);
    
    ValueShadow<bool> allowed_;
    ValueShadow<unsigned char> cliffDetect_;
    ValueShadow<unsigned char> leftDetect_;
    ValueShadow<unsigned char> rightDetect_;
    ValueShadow<unsigned char> leftWedge_;
    ValueShadow<unsigned char> rightWedge_;
    ValueShadow<unsigned char> backWedge_;

    void invalidate();
};

#endif  //  Decisions_h
