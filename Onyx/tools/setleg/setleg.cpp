#include "IK.h"
#include "ServoSet.h"
#include <iostream>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    if (argc != 5 && argc != 9 && argc != 13 && argc != 17) {
usage:
        std::cerr << "Usage: doik leg x y z" << std::endl;
        legparams lp = { 0 };
        get_leg_params(lp);
        std::cerr << "center: " << lp.center_x << "," << lp.center_y << "," << lp.center_z << std::endl;
        std::cerr << "lengths: " << lp.first_length << " " << lp.second_length << " " << lp.third_length << std::endl;
        exit(1);
    }
    ServoSet ss(true, boost::shared_ptr<Logger>());
    while (argc > 1) {
        int leg = atoi(argv[1]);
        float x = atof(argv[2]);
        float y = atof(argv[3]);
        float z = atof(argv[4]);
        if (leg < 0 || leg > 3) {
            goto usage;
        }
        legpose op = { 0 };
        bool ret = solve_leg(legs[leg], x, y, z, op);
        if (!ret) {
            std::cerr << "leg " << leg <<  "  error: " << solve_error << std::endl;
        }
        std::cerr << "leg " << leg << ": " << op.a << " " << op.b << " " << op.c << std::endl;

        for (int i = 0; i < 3; ++i) {
            ss.add_servo(i + leg * 3 + 1, 2048);
            ss.id(i + leg * 3 + 1).set_goal_position(((short *)&op.a)[i]);
        }
        argc -= 4;
        argv += 4;
    }
    ss.set_torque(1023, 1);
    ss.set_power(7);    //  power, servos, fans
    int n = 0;
    while (true) {
        ss.step();
        usleep(50000);
        ++n;
        if (n == 20) {
            unsigned char bytes[32] = { 0 };
            ss.slice_reg1(REG_PRESENT_TEMPERATURE, bytes, 32);
            for (int i = 1; i != 13; ++i) {
                std::cout << (int)bytes[i] << " ";
            }
            std::cout << std::endl;
            n = 0;
        }
    }
    return 0;
}
