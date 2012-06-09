
#include "Decisions.h"
#include "VideoCapture.h"
#include "Board.h"


Decisions::Decisions(
    MotorPowerBoard *mpb,
    Board *estop,
    SensorBoard *sens,
    UsbLinkBoard *ulb,
    VideoCapture *vc0,
    VideoCapture *vc1,
    UsbComm *uc,
    Talker *t)
{
    t->add_listener(this);
    allowed_.set(&mpb->allowed_);
    cliffDetect_.set(&sens->cliffDetect_);
    leftDetect_.set(&sens->leftDetect_);
    rightDetect_.set(&sens->rightDetect_);
    leftWedge_.set(&sens->leftWedge_);
    rightWedge_.set(&sens->rightWedge_);
    backWedge_.set(&sens->backWedge_);
}

void Decisions::invalidate()
{
    //  make some decision
    int turn = 0;
    int gas = 255;
    if (leftDetect_.get() < 20) {
        gas = -255;
        turn -= 50;
    }
    if (rightDetect_.get() < 20) {
        gas = -255;
        turn += 50;
        if (turn == 0) {
            turn = 50;
        }
    }
    if (leftWedge_.get() < 40) {
        turn -= (40 - leftWedge_.get());
    }
    if (rightWedge_.get() < 40) {
        turn += (40 - rightWedge_.get());
    }
    if (gas < 0) {
        if (rightWedge_.get() < 50) {
            gas = gas * rightWedge_.get() / 50;
        }
    }
    std::cout << "turn " << turn << " gas " << gas << std::endl;
}

