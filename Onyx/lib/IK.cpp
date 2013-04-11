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
    { CENTER_X, CENTER_Y, CENTER_Z,
        FIRST_LENGTH, 1,
        SECOND_LENGTH, -1,
        THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, 1,
        legori_forward, legori_90_up, legori_90_down  },
    { -CENTER_X, CENTER_Y, CENTER_Z,
        FIRST_LENGTH, 1,
        SECOND_LENGTH, -1,
        THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, 1,
        legori_forward, legori_90_up, legori_90_down  },
    { CENTER_X, -CENTER_Y, CENTER_Z, 
        FIRST_LENGTH, 1,
        SECOND_LENGTH, -1, 
        THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, 1,
        legori_forward, legori_90_up, legori_90_down  },
    { -CENTER_X, -CENTER_Y, CENTER_Z,
        FIRST_LENGTH, 1, 
        SECOND_LENGTH, -1,
        THIRD_LENGTH_A, THIRD_LENGTH_B, THIRD_LENGTH, 1,
        legori_forward, legori_90_up, legori_90_down  },
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
    fprintf(stderr, "dx %.1f dy %.1f dz %.1f\n", dx, dy, dz);
    //  orient the thigh in the direction of the target point
    float xylen = sqrtf(dx*dx + dy*dy);
    float xynorm = 1.0f / xylen;
    float ang0 = atan2f(dy, dx);
    fprintf(stderr, "ang0 %.1f xynorm %.1f\n", ang0, xynorm);
    //  Calculate distance from the oriented point.
    //  The fixed-length thigh bone points in the direction of the desired point
    float hdx = dx - dx * xynorm * leg.x0;
    float hdy = dy - dy * xynorm * leg.x0;
    fprintf(stderr, "hdx %.1f hdy %.1f\n", hdx, hdy);
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
    fprintf(stderr, "distance %.1f\n", distance);

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
    op.a = 2048 + ang0 * 2048 / M_PI;  //  assume ori right/outwards
    op.b = 2048 + ang1 * 2048 / M_PI;  //  assume ori right/outwards
    op.c = 2048 + ang2 * 2048 / M_PI;  //  assume ori down/outwards

    switch (leg.servo0) {
    case legori_forward:
        op.a -= 1024;
        break;
    case legori_45_out:
        op.a -= 512;
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
        op.b -= 1024;
        break;
    case legori_90_down:
        op.b += 1024;
        break;
    default:
        throw std::runtime_error("Bad legori for knee servo");
    }
    
    switch (leg.servo2) {
    case legori_90_out:
        op.c -= 1024;
        break;
    case legori_90_up:
        op.c -= 2048;
        break;
    case legori_90_down:
        //  do nothing
        break;
    default:
        throw std::runtime_error("Bad legori for knee servo");
    }
    if (op.a < 0 || op.a > 4096) {
        throw std::runtime_error("Resulting pose out of range for servo A.");
    }
    if (op.b < 0 || op.b > 4096) {
        throw std::runtime_error("Resulting pose out of range for servo A.");
    }
    if (op.c < 0 || op.c > 4096) {
        throw std::runtime_error("Resulting pose out of range for servo A.");
    }

    if ((leg.direction0 < 0) != flip) {
        op.a = 4096 - op.a;
    }
    if ((leg.direction1 < 0) != flip) {
        op.b = 4096 - op.b;
    }
    if ((leg.direction2 < 0) != flip) {
        op.c = 4096 - op.c;
    }

    return ret;
}



