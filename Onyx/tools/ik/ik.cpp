
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

struct leginfo {
    double cx;          //  center x in body space
    double cy;          //  center y in body space
    double cz;          //  center z in body space
    double x0;          //  what x extent at center for 1st joint
    double direction0;  //  positive (counterclockwise) or negative
    double x1;          //  x extent at center for 2nd joint
    double direction1;  //  positive (counterclockwise) or negative
    double x2;          //  x extent at center for 3rd joint
    double z2;          //  z extent at center for 3rd joint
    double l2;          //  length of x2/z2
    double direction2;  //  positive (counterclockwise) or negative
};

leginfo legs[] = {
    { CENTER_X, CENTER_Y, CENTER_Z,   FIRST_LENGTH, 1,  SECOND_LENGTH, 1,  THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, -1 },
    { -CENTER_X, CENTER_Y, CENTER_Z,  FIRST_LENGTH, -1, SECOND_LENGTH, -1, THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, 1 },
    { CENTER_X, -CENTER_Y, CENTER_Z,  FIRST_LENGTH, -1, SECOND_LENGTH, 1,  THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, -1 },
    { -CENTER_X, -CENTER_Y, CENTER_Z, FIRST_LENGTH, 1,  SECOND_LENGTH, -1, THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, 1 },
};

struct legpose {
    unsigned short a;
    unsigned short b;
    unsigned short c;
};

std::string solve_error;

//  Solve the leg to point to the given position.
//  If there is no acceptable solution, false is returned, 
//  and some pose approximating the general direction intended 
//  is returned as the "solution."
bool solve_leg(leginfo const &leg, double x, double y, double z, legpose &op) {
    double dx = x - leg.cx;
    double dy = y - leg.cy;
    double dz = z - leg.cz;
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
    double angle0 = atan2(dy, dx);
    //  now position joint 1/2 in the given plane
    double sn = sin(angle0);
    double cs = cos(angle0);
    double ndx = dx * cs + dy * sn;
    double ndy = -dx * sn + dy * cs;
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
    double d = sqrt(ndx*ndx + dz*dz);
    double md = leg.l2 - leg.x1;
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
            double r = md / d;
            ndx = ndx * r;
            dz = dz * r;
        }
        ret = false;
    }
    //  check maximum reach
    md = leg.l2 + leg.x1;
    if (d > md) {
        //  cannot reach the destination
        double r = md / d;
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

    double A = 2*dz/(-1*ndx);
    double B = (leg.l2*leg.l2 - leg.x1*leg.x1 - ndx*ndx - dz*dz)/(-2*ndx);
    double a = (A*A+1);
    double b = 2 * A * B;
    double c = B*B - leg.x1*leg.x1;
    double inner = b*b - 4*a*c;
    double root = 0;
    if (inner < 0) {
        ret = false;
        solve_error = "root of negative";
        root = leg.x1;  //  straight up
    }
    else {
        root = (-b + sqrt(b*b - 4*a*c))/(2 * a);
    }
    double out = sqrt(leg.x1*leg.x1-root*root);
    double angle1 = atan2(root, out);
    ndx -= out;
    dz -= root;
    double hpi = M_PI * 0.5;
    double qpi = M_PI * 0.25;
    double pi = M_PI;
    double angle2 = atan2(dz, ndx) + hpi - atan2(leg.x2, leg.z2) - angle1;
    
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

    op.a = (unsigned short)(2048 + angle0 * 2047 / pi * leg.direction0);
    op.b = (unsigned short)(2048 + angle1 * 2047 / pi * leg.direction1);
    op.c = (unsigned short)(2048 + angle2 * 2047 / pi * leg.direction2);

    return ret;
}


struct initinfo {
    unsigned short id;
    unsigned short center;
};
static const initinfo init[] = {
    { 1, 2048+512 },
    { 2, 2048+512 },
    { 3, 2048+512 },
    { 4, 2048-512 },
    { 5, 2048-512 },
    { 6, 2048-512 },
    { 7, 2048-512 },
    { 8, 2048+512 },
    { 9, 2048+512 },
    { 10, 2048+512 },
    { 11, 2048-512 },
    { 12, 2048-512 },
};

int main() {
    ServoSet ss;
    for (size_t i = 0; i < sizeof(init)/sizeof(init[0]); ++i) {
        ss.add_servo(init[i].id, init[i].center);
    }

    int step = 0;

    bool prevsolved = true;
    while (true) {
        //for (int leg = 0; leg < 4; ++leg) {
            int leg = step & 3;
            double xpos = CENTER_X /*+ FIRST_LENGTH*/ + SECOND_LENGTH;
            if (leg & 1) {
                xpos = -xpos;
            }
            double ypos = CENTER_Y + FIRST_LENGTH;
            if (leg & 2) {
                ypos = -ypos;
            }
            double zpos = -80;
            ypos += 50 * sin(step / 1000.0);
            legpose oot;
            bool solved = solve_leg(legs[leg], xpos, ypos, zpos, oot);
            if (solved != prevsolved) {
                if (!solved) {
                    std::cerr << "not solved: " << solve_error << std::endl;
                }
                else {
                    std::cerr << "solved" << std::endl;
                }
                prevsolved = solved;
            }
            ss.id(1 + 3*leg).set_goal_position(oot.a);
            ss.id(2 + 3*leg).set_goal_position(oot.b);
            ss.id(3 + 3*leg).set_goal_position(oot.c);
        //}

        ss.step();
        if (ss.queue_depth() > 30) {
            std::cerr << "queue_depth: " << ss.queue_depth() << std::endl;
            while (ss.queue_depth() > 0) {
                ss.step();
            }
        }
        usleep(1000);
        step += 3;
        if (step >= 6283) {
            step -= 6283;
        }
    }
}

