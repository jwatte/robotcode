#include <iostream>
#include <sstream>
#include "Transform.h"
#include "IK.h"
#include "ServoSet.h"

#include <ncurses.h>


#define COL_WID 20

void display_at(int x, int y, std::string const &s) {
    move(y, x);
    addstr(s.c_str());
}

int changecount[16] = { 0 };
unsigned short prev[16] = { 0 };


int main() {
    initscr();

    ServoSet ss(true, boost::shared_ptr<Logger>());
    ss.set_power(7);    //  all except guns
    ss.step();
    ss.step();
    for (int i = 1; i != 13; ++i) {
        ss.add_servo(i, 2048);
    }
    ss.set_torque(500, 1);
    for (int i = 0; i != 50; ++i) {
        ss.step();
        usleep(10000);
    }
    for (int i = 1; i != 13; ++i) {
        ss.id(i).set_reg1(REG_TORQUE_ENABLE, 0);
    }
    while (true) {
        ss.step();
        standend();
        clear();
        unsigned char buf[16] = { 0 };
        ss.slice_reg1(REG_PRESENT_TEMPERATURE, buf, 16);
        std::stringstream sts;
        for (int i = 1; i != 13; ++i) {
            sts << (int)buf[i] << "C ";
        }
        display_at(0, 7, sts.str());
        unsigned short slice[16] = { 0 };
        ss.slice_reg2(REG_PRESENT_POSITION, slice, 16);
        for (int i = 1; i != 13; ++i) {
            std::stringstream sts;
            sts << slice[i];
            if (abs(prev[i] - slice[i]) > 1) {
                changecount[i] = 20;
                prev[i] = slice[i];
            }
            if (changecount[i] > 0) {
                --changecount[i];
                standout();
            }
            display_at((i-1)/3*COL_WID, (i-1)%3, sts.str());
            standend();
        }
        for (int i = 0; i != 4; ++i) {
            legpose lp = { slice[1+i*3], slice[2+i*3], slice[3+i*3] };
            float x, y, z;
            forward_leg(legs[i], lp, x, y, z);
            std::stringstream sts;
            sts << (int)x << "," << (int)y << "," << (int)z;
            display_at(i*COL_WID, 5, sts.str());
        }
        refresh();
        usleep(50000);
    }
}

