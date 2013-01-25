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
class SlewRateInterpolator :
    public TimeInterpolator<T> {
public:
    SlewRateInterpolator(T t, T slewRate, double slewTime, double now) :
        TimeInterpolator<T>(t, now),
        slew_((double)slewTime / (double)slewRate)
    {
    }
    void setTarget(T val)
    {
        double dt = fabs((double)(val - TimeInterpolator<T>::value_) / slew_);
        setTarget(val, TimeInterpolator<T>::nowTime_ + dt);
    }
    double slew_;
};

#endif  //  lib_Interpolator_h
