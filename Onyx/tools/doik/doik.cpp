#include "IK.h"
#include <iostream>
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: doik x y z" << std::endl;
        legparams lp = { 0 };
        get_leg_params(lp);
        std::cerr << "center: " << lp.center_x << "," << lp.center_y << "," << lp.center_z << std::endl;
        std::cerr << "lengths: " << lp.first_length << " " << lp.second_length << " " << lp.third_length << std::endl;
        exit(1);
    }
    float x = atof(argv[1]);
    float y = atof(argv[2]);
    float z = atof(argv[3]);
    legpose op = { 0 };
    bool ret = solve_leg(legs[0], x, y, z, op);
    if (!ret) {
        std::cerr << solve_error << std::endl;
    }
    std::cerr << op.a << " " << op.b << " " << op.c << std::endl;
    return ret ? 0 : 1;
}
