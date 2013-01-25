#if !defined(lib_Interpolator_h)
#define lib_Interpolator_h

#include <math.h>

template<typename T>
class TimeInterpolator {
public:
    TimeInterpolator(T t, double time) :
        value_(t),
        prevValue_(t),
        nextValue_(t),
        nowTime_(time),
        prevTime_(time),
        nextTime_(time) {
    }
    void setTarget(T val, double atTime) {
        prevTime_ = nowTime_;
        prevValue_ = value_;
        nextTime_ = atTime;
        nextValue_ = val;
        inverseTimeDelta_ = nextTime_ - prevTime_;
        if (inverseTimeDelta_ != 0) {
            inverseTimeDelta_ = 1.0 / inverseTimeDelta_;
        }
    }
    void setTime(double time) {
        nowTime_ = time;
        if (nowTime_ > nextTime_) {
            value_ = nextValue_;
        }
        else {
            value_ = (T)(nextValue_ + (prevValue_ - nextValue_) * (nextTime_ - time) *
                inverseTimeDelta_);
        }
    }
    T get() const {
        return value_;
    }
    T value_;
    T prevValue_;
    T nextValue_;
    double nowTime_;
    double prevTime_;
    double nextTime_;
    double inverseTimeDelta_;
};

template<typename T>
class SlewRateInterpolator {
public:
    SlewRateInterpolator(T t, T slewRate, double slewTime, double now) :
        value_(t),
        target_(t),
        timeNow_(now),
        slew_((double)slewRate / (double)slewTime)
    {
    }
    void setTarget(T val) {
        target_ = val;
    }
    void setTime(double t) {
        double dt = t - timeNow_;
        timeNow_ = t;
        if (dt < 0 || dt > 1) {
            value_ = target_;
        }
        else {
            if (value_ > target_ + slew_ * dt) {
                value_ = value_ - slew_ * dt;
            }
            else if (value_ < target_ - slew_ * dt) {
                value_ = value_ + slew_ * dt;
            }
            else {
                value_ = target_;
            }
        }
    }
    T get() const {
        return value_;
    }
    T value_;
    T target_;
    double timeNow_;
    double slew_;
};

#endif  //  lib_Interpolator_h
