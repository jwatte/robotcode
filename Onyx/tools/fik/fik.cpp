#include "IK.h"
#include <iostream>
#include "Transform.h"
#include <stdlib.h>

int main(int argc, char const *argv[]) {
    if (argc != 5) {
usage:
        std::cerr << "usage: fik leg a b c" << std::endl;
        return 1;
    }
    int leg = atoi(argv[1]);
    legpose lp = { (unsigned short)atoi(argv[2]), (unsigned short)atoi(argv[3]), (unsigned short)atoi(argv[4]) };
    float ox, oy, oz;
    if (leg < 0 || leg > 3) {
        goto usage;
    }
    if (lp.a > 4095 || lp.b > 4095 || lp.c > 4095) {
        goto usage;
    }
    forward_leg(legs[leg], lp, ox, oy, oz);
    std::cerr << vec4(ox, oy, oz) << std::endl;
    return 0;
}
