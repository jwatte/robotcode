
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
};

struct leginfo {
    //  horizontal pivot location
    float cx;           //  center x in body space
    float cy;           //  center y in body space
    float cz;           //  center z in body space

    //  leg segment dimensions

    float x0;           //  what extent at center for 1st joint
    float direction0;   //  positive (counterclockwise, inward) or negative
    float delta0;       //  delta angle 0
    float min0;         //  in radians
    float max0;         //  in radians

    float x1;           //  extent at center for 2nd joint
    float direction1;   //  positive (counterclockwise, up) or negative
    float delta1;       //  delta angle 1
    float min1;         //  in radians
    float max1;         //  in radians

    float l2;           //  length of x2/z2
    float direction2;   //  positive (counterclockwise, up/out) or negative
    float delta2;       //  delta angle 2
    float min2;         //  in radians
    float max2;         //  in radians

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

