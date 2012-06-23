
#include "Decisions.h"
#include "VideoCapture.h"
#include "Board.h"
#include "DecisionPanel.h"


Decisions::Decisions(
    MotorPowerBoard *mpb,
    Board *estop,
    SensorBoard *sens,
    UsbLinkBoard *ulb,
    AsyncVideoCapture *avc,
    Talker *t, 
    DecisionPanel *panel)
{
    motorPowerBoard_ = mpb;
    t->add_listener(this);
    allowed_.set(&mpb->allowed_);
    cliffDetect_.set(&sens->cliffDetect_);
    leftDetect_.set(&sens->leftDetect_);
    rightDetect_.set(&sens->rightDetect_);
    leftWedge_.set(&sens->leftWedge_);
    rightWedge_.set(&sens->rightWedge_);
    backWedge_.set(&sens->backWedge_);
    panel_ = panel;
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
    unsigned char d[2] = {
        gas >> 1, turn >> 1
    };
    motorPowerBoard_->write_reg(0, 2, d);
    DecisionPanelData dpd;
    dpd.gas_ = gas;
    dpd.turn_ = turn;
    dpd.cliff_ = cliffDetect_.get();
    dpd.leftWheel_ = leftDetect_.get();
    dpd.rightWheel_ = rightDetect_.get();
    dpd.leftWedge_ = leftWedge_.get();
    dpd.rightWedge_ = rightWedge_.get();
    dpd.backWedge_ = backWedge_.get();
    panel_->setData(dpd);
}

