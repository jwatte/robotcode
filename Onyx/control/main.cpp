
#include "itime.h"
#include "istatus.h"
#include "inetwork.h"
#include "util.h"
#include "protocol.h"
#include <iostream>

unsigned short port = 6969;

ITime *itime;
IStatus *istatus;
ISockets *isocks;
INetwork *inet;
IPacketizer *ipacketizer;


void dispatch(unsigned char type, size_t size, void const *data) {
    std::cerr << "packet type " << hexnum(type) << " size " << size << std::endl;
}

void scan_for_robots() {
    P_Discover pd;
    ipacketizer->send(C2R_Discover, sizeof(pd), &pd);
}

int main(int argc, char const *argv[]) {

    itime = newclock();
    istatus = mkstatus();
    isocks = mksocks(port, istatus);
    inet = scan(isocks, itime, istatus);
    ipacketizer = packetize(inet, istatus);

    double then = itime->now();
    double bc = 0;
    while (true) {
        double now = itime->now();
        double dt = now - then;
        if (dt < 0.015) {
            itime->sleep(0.015 - dt);
            now = itime->now();
        }
        if (now > bc) {
            scan_for_robots();
            bc = now + 1;
        }
        ipacketizer->step();
        unsigned char type = 0;
        size_t size = 0;
        void const *data = 0;
        while (ipacketizer->receive(type, size, data)) {
            dispatch(type, size, data);
        }
    }

    return 1;
}
