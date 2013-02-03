#include "IK.h"
#include "ServoSet.h"
#include <math.h>
#include <boost/lexical_cast.hpp>

#define CENTER_X 75
#define CENTER_Y 62
#define CENTER_Z 26
#define FIRST_LENGTH 85
#define SECOND_LENGTH 73
#define THIRD_LENGTH 170
#define THIRD_LENGTH_A 45
#define THIRD_LENGTH_B 164

leginfo legs[] = {
    { CENTER_X, CENTER_Y, CENTER_Z,   FIRST_LENGTH, 1,  SECOND_LENGTH, -1,  THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, 1 },
    { -CENTER_X, CENTER_Y, CENTER_Z,  FIRST_LENGTH, -1, SECOND_LENGTH, 1, THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, -1 },
    { CENTER_X, -CENTER_Y, CENTER_Z,  FIRST_LENGTH, -1, SECOND_LENGTH, -1,  THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, 1 },
    { -CENTER_X, -CENTER_Y, CENTER_Z, FIRST_LENGTH, 1,  SECOND_LENGTH, 1, THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, -1 },
};


//  how much to compensate first joint for based on servo orientation?
static short const leg_comp[] = {
    0,
    1024,
    512
};
static LegConfiguration g_lc;

void set_leg_configuration(LegConfiguration lc) {
    g_lc = lc;
}

void get_leg_params(legparams &op) {
    op.center_x = CENTER_X;
    op.center_y = CENTER_Y;
    op.center_z = CENTER_Z;
    op.first_length = FIRST_LENGTH;
    op.second_length = SECOND_LENGTH;
    op.third_length = THIRD_LENGTH;
    op.third_length_x = THIRD_LENGTH_A;
    op.third_length_z = THIRD_LENGTH_B;
}

std::string solve_error;

//  Solve the leg to point to the given position.
//  If there is no acceptable solution, false is returned, 
//  and some pose approximating the general direction intended 
//  is returned as the "solution."
bool solve_leg(leginfo const &leg, float x, float y, float z, legpose &op) {
    float dx = x - leg.cx;
    float dy = y - leg.cy;
    float dz = z - leg.cz;
    bool ret = true;
    if (leg.cx < 0) {
        dx *= -1;
    }
    if (leg.cy < 0) {
        dy *= -1;
    }
    if (dx <= 0) {
        if (dy < 20) {
            if (dy < 0) {
                dx = 20;
            }
            else {
                dy = 20;
            }
        }
        ret = false;
        solve_error = "negative dx";
    }
    float angle0 = atan2f(dy, dx);
    //  now position joint 1+2 in the given plane
    float sn = sinf(angle0);
    float cs = cosf(angle0);
    float ndx = dx * cs + dy * sn;
    float ndy = -dx * sn + dy * cs;
    //  this should have projected to the y=0 plane
    assert(fabs(ndy) < 1e-3);
    ndx -= leg.x0;

    //  now, ndx and dz is a reference plane, and I have the two "sticks" x1 
    //  and x2/z2 to position to reach the given point
    if (dz > -leg.cz) {
        //  don't allow movement into the joint1 servo
        if (ndx < 0) {
            ndx = 0;
            ret = false;
            solve_error = "z movement into middle servo";
        }
    }
    if (dz > ndx) {
        //  select the lower-right half-plane
        dz = ndx;
        ret = false;
        solve_error = "z movement above diagonal half-plane";
    }
    //  check minimum reach
    float d = sqrtf(ndx*ndx + dz*dz);
    float md = fabsf(leg.l2 - leg.x1);
    /*
    if (d < md) {
        solve_error = "too close to joint; ";
        solve_error += "d=" + boost::lexical_cast<std::string>(d);
        solve_error += "; md=" + boost::lexical_cast<std::string>(md);
        solve_error += "; ndx=" + boost::lexical_cast<std::string>(ndx);
        solve_error += "; dz=" + boost::lexical_cast<std::string>(dz);
        //  cannot possibly get within the inner circle
        if (d == 0) {
            ndx = md;
        }
        else {
            float r = md / d;
            ndx = ndx * r;
            dz = dz * r;
        }
        ret = false;
    }
    */
    //  check maximum reach
    md = leg.l2 + leg.x1;
    if (d > md) {
        //  cannot reach the destination
        float r = md / d;
        ndx = ndx * r;
        dz = dz * r;
        ret = false;
        solve_error = "too far from joint";
    }

    //  Now, I have a reasonably sane position.
    //  A circle centered on origin with radius x1 intersects 
    //  a circle centered on target with radius l2.
    //  Choose the upper of those intersections as the joint2 location.
    //  We are looking for jx1 and jz1.
    //  We know x1, l2, jx2 and jz2.
    //  1. jx1*jx1 + jz1*jz1 == x1*x1
    //  2. (jx2-jx1)*(jx2-jx1) + (jz2-jz1)*(jz2-jz1) == l2*l2
    //  2. jx2*jx2 - 2*jx1*jx2 + jx1*jx1 + jz2*jz2 - 2*jz1*jz2 + jz1*jz1 == l2*l2
    //  subtract 1. from 2. go give
    //  3. jx2*jx2 - 2*jx1*jx2 + jz2*jz2 - 2*jz1*jz2 == l2*l2 - x1*x1
    //  this gives us a line, re-arranging constants on the right:
    //  3. -2*jx2* jx1 + -2*jz2 * jz1 == l2*l2 - x1*x1 - jx2*jx2 - jz2*jz2
    //  3. -2*jx2* jx1 == l2*l2 - x1*x1 - jx2*jx2 - jz2*jz2 + 2*jz2 * jz1
    //  3. jx1 == (2*jz2)/(-2*jx2) * jz1 + (l2*l2 - x1*x1 - jx2*jx2 - jz2*jz2)/(-2*jx2)
    //  Now, establish A = (2*jz2)/(-2*jx2), B = (l2*l2 - x1*x1 - jx2*jx2 - jz2*jz2)/(-2*jx2)
    //  3. jx1 == A jz1 + B
    //  Substitute into 1 to yield:
    //  4. (A jz1 + B)(A jz1 + B) + jz1 * jz1 == x1*x1
    //  4. (A*A+1) jz1*jz1 + 2AB jz1 + B*B - x1*x1 == 0
    //  This, we can solve with a simple quadratic formula, picking the positive root.

    float A = 2*dz/(-1*ndx);
    float B = (leg.l2*leg.l2 - leg.x1*leg.x1 - ndx*ndx - dz*dz)/(-2*ndx);
    float a = (A*A+1);
    float b = 2 * A * B;
    float c = B*B - leg.x1*leg.x1;
    float inner = b*b - 4*a*c;
    float root = 0;
    if (inner < 0) {
        ret = false;
        solve_error = "root of negative";
        root = leg.x1;  //  straight up
    }
    else {
        root = (-b + sqrtf(b*b - 4*a*c))/(2 * a);
    }
    float out = sqrtf(leg.x1*leg.x1-root*root);
    float angle1 = atan2f(root, out);
    ndx -= out;
    dz -= root;
    float hpi = M_PI * 0.5;
    float qpi = M_PI * 0.25;
    float pi = M_PI;
    float angle2 = atan2f(dz, ndx) + hpi - atan2f(leg.x2, leg.z2) - angle1;
    
    //  limit to "safe" movement angles
    if (angle0 < -qpi) {
        angle0 = -qpi;
        ret = false;
        solve_error = "too small angle0";
    }
    if (angle0 > hpi) {
        angle0 = hpi;
        ret = false;
        solve_error = "too large angle0";
    }
    if (angle1 < -qpi) {
        angle1 = -qpi;
        ret = false;
        solve_error = "too small angle1";
    }
    if (angle1 > hpi) {
        angle1 = hpi;
        ret = false;
        solve_error = "too large angle1";
    }
    if (angle2 < -hpi) {
        angle2 = -hpi;
        ret = false;
        solve_error = "too small angle2";
    }
    if (angle2 > hpi) {
        angle2 = hpi;
        ret = false;
        solve_error = "too large angle2";
    }

    op.a = (unsigned short)(2048 + angle0 * 2047 / pi * leg.direction0 - leg.direction0 * leg_comp[g_lc]);
    op.b = (unsigned short)(2048 + angle1 * 2047 / pi * leg.direction1);
    op.c = (unsigned short)(2048 + angle2 * 2047 / pi * leg.direction2);

    return ret;
}



