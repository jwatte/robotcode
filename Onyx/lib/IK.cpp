#include "IK.h"
#include "ServoSet.h"
#include <math.h>
#include <boost/lexical_cast.hpp>

#define CENTER_X 45
#define CENTER_Y 62
#define CENTER_Z 80
#define FIRST_LENGTH 60
#define SECOND_LENGTH 73
#define THIRD_LENGTH 151
#define THIRD_LENGTH_A 15
#define THIRD_LENGTH_B 150

leginfo legs[] = {
    { CENTER_X, CENTER_Y, CENTER_Z,   FIRST_LENGTH, -1,  SECOND_LENGTH, -1,  THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, -1, legori_forward, legori_90_up, legori_90_down  },
    { -CENTER_X, CENTER_Y, CENTER_Z,  FIRST_LENGTH, -1, SECOND_LENGTH, -1, THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, -1, legori_forward, legori_90_up, legori_90_down  },
    { CENTER_X, -CENTER_Y, CENTER_Z,  FIRST_LENGTH, -1, SECOND_LENGTH, -1,  THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, -1, legori_forward, legori_90_up, legori_90_down  },
    { -CENTER_X, -CENTER_Y, CENTER_Z, FIRST_LENGTH, -1,  SECOND_LENGTH, -1, THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, -1, legori_forward, legori_90_up, legori_90_down  },
};


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
//  X -> right
//  Y -> forward
//  Z -> up
bool solve_leg(leginfo const &leg, float x, float y, float z, legpose &op) {
    const float hpi = M_PI * 0.5;
    //const float qpi = M_PI * 0.25;
    //const float pi = M_PI;

    float dx = x - leg.cx;
    float dy = y - leg.cy;
    float dz = z - leg.cz;
    //  solve as if it's the front-right leg
    bool ret = true;
    bool flip = false;
    if (leg.cx < 0) {   //  left side
        dx *= -1;
        flip = !flip;
    }
    if (leg.cy < 0) {   //  back side
        dy *= -1;
        flip = !flip;
    }

    //  orientation:
    //               Ankle
    //               /  |
    //              x1  |
    //             /   l2 
    //  Hip--x0--Knee   |
    //                  |

    if (dx < 0) {
        //  don't allow poses that turn the legs inside the body
        dx = 0;
    }
    if (dz > 0) {
        //  don't allow poses that lift legs above the hips
        dz = 0;
    }
    if (fabsf(dx) < 1e-3 && fabsf(dy) < 1e-3) {
        dx = 1e-3;
    }
    //  orient the thigh in the direction of the target point
    float xylen = sqrtf(dx*dx + dy*dy);
    float xynorm = 1.0f / xylen;
    float ang0 = atan2f(dy, dx);
    //  calculate distance from the oriented point
    float hdx = dx - dx * xynorm * leg.x0;
    float hdy = dy - dy * xynorm * leg.x0;
    float maxreach = leg.x1 + leg.l2;
    float minreach = fabsf(leg.x1 - leg.l2);
    float distance = sqrtf(hdx*hdx + hdy*hdy + dz*dz);
    if (distance < 1e-3) {
        dz = -1e-3;
    }
    if (distance > maxreach) {
        ret = false;
        solve_error = "Leg cannot reach far from body.";
        //  fix up by moving closer to body
        hdx *= maxreach / distance * 0.9999;
        hdy *= maxreach / distance * 0.9999;
        dz *= maxreach / distance * 0.9999;
        distance = sqrtf(hdx*hdx + hdy*hdy + dz*dz);
    }
    if (distance < minreach) {
        ret = false;
        solve_error = "Leg cannot reach close to knee.";
        //  fix up by moving leg downwards
        if (dz > 0) {
            dz = minreach;
        }
        else {
            dz = -minreach;
        }
        distance = sqrtf(hdx*hdx + hdy*hdy + dz*dz);
    }
    assert(distance >= minreach && distance <= maxreach);

    //  Now I have a triangle, from knee joint, to ankle joint, to foot contact point.
    //  I know the length of each of the sides; now I want to know the coordinates of 
    //  the sides so I can calculate the angles.
    //  a == distance, b == x1, c == l2
    //  A = angle at ankle, B = angle at foot, C = angle at knee (from foot, to ankle)
    //  Cosine rule: a2 = b2 + c2 - 2bc cos A.
    float cosA = (leg.x1*leg.x1 + leg.l2*leg.l2 - distance*distance) / (2 * leg.x1 * leg.l2);
    assert(cosA <= 1.0f);
    float ang2 = acosf(cosA);
    float cosC = (leg.x1*leg.x1 + distance*distance - leg.l2*leg.l2) / (2 * leg.x1 * distance);
    assert(cosC <= 1.0f);
    float angC = acosf(cosC);
    //  Now, I actually know the right triangle from point of contact, to below knee, to knee
    float xydist = sqrtf(hdx*hdx + hdy+hdy);
    float Bprime = atan2f(xydist, -dz);
    float ang1 = angC - Bprime;
    if (ang1 > hpi) {
        assert(angC < M_PI);
        ang1 += (angC - M_PI) * 2;
        assert(ang1 <= hpi);
    }

    //  Now, orient the output solution based on the orientation of the servos
    op.a = 2048 + ang0 * 2048 / M_PI * leg.direction0;  //  assume ori right/outwards
    op.b = 2048 + ang1 * 2048 / M_PI * leg.direction1;  //  assume ori right/outwards
    op.c = 2048 + ang2 * 2048 / M_PI * leg.direction2;  //  assume ori down/outwards

    switch (leg.servo0) {
    case legori_forward:
        op.a -= 1024 * leg.direction0;
        break;
    case legori_45_out:
        op.a -= 512 * leg.direction0;
        break;
    case legori_90_out:
        //  do nothing
        break;
    default:
        throw std::runtime_error("Bad legori for hip servo");
    }
    
    switch (leg.servo1) {
    case legori_90_out:
        //  do nothing
        break;
    case legori_90_up:
        op.b -= 1024 * leg.direction1;
        break;
    case legori_90_down:
        op.b += 1024 * leg.direction1;
        break;
    default:
        throw std::runtime_error("Bad legori for knee servo");
    }
    
    switch (leg.servo2) {
    case legori_90_out:
        op.c -= 1024 * leg.direction2;
        break;
    case legori_90_up:
        op.c -= 2048 * leg.direction2;
        if (op.c < 0 || op.c > 4095) {
            ret = false;
            op.c = 0;
            solve_error = "Cannot move inside 0 point in ankle.";
        }
        break;
    case legori_90_down:
        //  do nothing
        break;
    default:
        throw std::runtime_error("Bad legori for knee servo");
    }

    return ret;

/*
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
        solve_error = "negative dx into robot";
    }

    //  what's the angle of the closest leg?
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

    int dir = leg.direction0;
    if (flip) {
        dir = -dir;
    }
    op.a = (unsigned short)(((angle0 * 2048 / pi) * dir) + 2048);
    if (op.a < 0) {
        op.a = 0;
        ret = false;
        solve_error = "negative A joint solution";
    }
    if (op.a > 4095) {
        op.a = 4095;
        ret = false;
        solve_error = "too large A joint solution";
    }

    dir = leg.direction1;
    if (flip) {
        dir = -dir;
    }
    op.b = (unsigned short)((angle1 * 2048 / pi + 512) * dir + 2048);
    if (op.b < 0) {
        op.b = 0;
        ret = false;
        solve_error = "negative B joint solution";
    }
    if (op.b > 4095) {
        op.b = 4095;
        ret = false;
        solve_error = "too large B joint solution";
    }

    dir = leg.direction2;
    if (flip) {
        dir = -dir;
    }
    op.c = (unsigned short)((angle2 * 2048 / pi + 512) * dir + 2048);
    if (op.c < 0) {
        op.c = 0;
        ret = false;
        solve_error = "negative C joint solution";
    }
    if (op.c > 4095) {
        op.c = 4095;
        ret = false;
        solve_error = "too large C joint solution";
    }

    return ret;
    */
}



