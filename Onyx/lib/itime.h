#if !defined(robot_time_h)
#define robot_time_h

class ITime {
public:
    virtual double now() = 0;
    virtual void sleep(double t) = 0;
};


ITime *newclock();

#endif  //  robot_time_h

