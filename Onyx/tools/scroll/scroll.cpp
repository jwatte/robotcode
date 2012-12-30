
#include "ServoSet.h"
#include <iostream>
#include <boost/lexical_cast.hpp>

int main(int argc, char const *argv[]) {
    if (argc < 2 || argc > 4) {
        std::cerr << "usage: scroll <id> [left right]" << std::endl;
        return 1;
    }
    int id = boost::lexical_cast<int>(argv[1]);
    if (id < 1 || id > 253) {
        std::cerr << "valid id is in 1 .. 253; got " << id << std::endl;
        return 1;
    }
    int left = 2048 - 200;
    int right = 2048 + 200;
    if (argv[2]) {
        left = boost::lexical_cast<int>(argv[2]);
        if (argv[3]) {
            right = boost::lexical_cast<int>(argv[3]);
        }
    }
    if (left > right || left < 0 || right > 4095) {
        std::cerr << "left .. right must be in 0 .. 4095; got " << left << " .. " << right << std::endl;
        return 1;
    }
    ServoSet ss;
    Servo &s(ss.add_servo(id));
    unsigned short pos = (left + right) / 2;
    short delta = -1;
    while (true) {
        ss.step();
        usleep(1000);
        s.set_goal_position(pos);
        pos += delta;
        if (pos <= left) {
            std::cerr << "left: " << left << std::endl;
            delta = 1;
            pos = left;
        }
        if (pos >= right) {
            std::cerr << "right: " << right << std::endl;
            delta = -1;
            pos = right;
        }
    }
}

