
#if !defined(Onyx_IK_h)
#define Onyx_IK_h

#include <string>
#include <stdlib.h>

struct legparams {
    float center_x;
    float center_y;
    float center_z;
    float first_length;
    float second_length;
    float third_length;
    float third_length_x;
    float third_length_z;
};

enum legori {
    legori_forward, legori_45_out, legori_90_out, legori_90_up, legori_90_down
};

struct leginfo {
    float cx;           //  center x in body space
    float cy;           //  center y in body space
    float cz;           //  center z in body space
    float x0;           //  what x extent at center for 1st joint
    float direction0;   //  positive (counterclockwise, inward) or negative
    float x1;           //  x extent at center for 2nd joint
    float direction1;   //  positive (counterclockwise, up) or negative
    float x2;           //  x extent at center for 3rd joint
    float z2;           //  z extent at center for 3rd joint
    float l2;           //  length of x2/z2
    float direction2;   //  positive (counterclockwise, up/out) or negative
    legori servo0;      //  orientation of "center" of first servo
    legori servo1;      //  orientation of "center" of second servo
    legori servo2;      //  orientation of "center" of third servo
};

struct legpose {
    unsigned short a;
    unsigned short b;
    unsigned short c;
};


extern leginfo legs[];
extern std::string solve_error;

extern bool solve_leg(leginfo const &leg, float x, float y, float z, legpose &op);
extern void get_leg_params(legparams &op);

#endif  //  Onyx_IK_h

