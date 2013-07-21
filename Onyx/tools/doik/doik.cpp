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
    bool allok = true;
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
            allok = false;
        }
        else {
            float ox = 0, oy = 0, oz = 0;
            forward_leg(legs[leg], op, ox, oy, oz);
            if (fabsf(ox-x) > 2 || fabsf(oy-y) > 2 || fabsf(oz-z) > 2) {
                std::cerr << "position after solution does not match input: " <<
                    "x=" << ox << " (" << x << "), y=" << oy << " (" << y <<
                    ") z=" << oz << " (" << z << ")" << std::endl;
                allok = false;
            }
        }
        std::cerr << "leg " << leg << ": " << op.a << " " << op.b << " " << op.c << std::endl;
        argc -= 4;
        argv += 4;
    }
    return allok ? 0 : 1;
}
