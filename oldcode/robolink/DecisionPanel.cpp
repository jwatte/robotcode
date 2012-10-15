#include "DecisionPanel.h"

#include <FL/Fl_Group.H>
#include <FL/Fl_Dial.H>
#include "BarGauge.h"


DecisionPanel::DecisionPanel() :
    group_(0),
    gas_(0),
    turn_(0)
{
}

void DecisionPanel::init()
{
    group_ = new Fl_Group(0, 0, 200, 200);
    gas_ = new Fl_Dial(10, 10, 85, 85);
    gas_->minimum(-255);
    gas_->maximum(255);
    gas_->type(FL_LINE_DIAL);
    turn_ = new Fl_Dial(105, 10, 85, 85);
    turn_->minimum(-127);
    turn_->maximum(127);
    turn_->type(FL_LINE_DIAL);
    cliff_ = new BarGauge(50, 105, 100, 15);
    left_ = new BarGauge(10, 125, 85, 15);
    right_ = new BarGauge(105, 125, 85, 15);
    leftWedge_ = new BarGauge(10, 145, 85, 15);
    rightWedge_ = new BarGauge(105, 145, 85, 15);
    backWedge_ = new BarGauge(50, 165, 85, 15);
    group_->end();
}

void DecisionPanel::setData(DecisionPanelData const &dpd)
{
    gas_->value(dpd.gas_);
    turn_->value(dpd.turn_);
    cliff_->value(dpd.cliff_);
    left_->value(dpd.leftWheel_);
    right_->value(dpd.rightWheel_);
    leftWedge_->value(dpd.leftWedge_);
    rightWedge_->value(dpd.rightWedge_);
    backWedge_->value(dpd.backWedge_);
}

