
#include "itime.h"
#include "util.h"
#include <time.h>
#include <unistd.h>
#include <stdexcept>

class Time : public ITime {
public:
    double now() {
        return read_clock();
    }
    void sleep(double t) {
        if (t > 1) {
            throw std::runtime_error("Sleeping for a second or more is a really bad idea.");
        }
        else {
            usleep((long)(t * 1000000));
        }
    }
};


ITime *newclock() {
    return new Time();
}


