#include "IK.h"
#include "ServoSet.h"
#include <math.h>
#include <boost/lexical_cast.hpp>

#define CENTER_X 45
#define CENTER_Y 62
#define CENTER_Z 40

#define FIRST_LENGTH 60
#define SECOND_LENGTH 80
#define THIRD_LENGTH 161
#define THIRD_HORIZONTAL 50

#define THIRD_ANGLE (acosf(THIRD_HORIZONTAL/THIRD_LENGTH))

//  The direction constant is additionally 
//  multiplied by sgn(xpos)*sgn(ypos)
leginfo legs[] = {
    {   CENTER_X, CENTER_Y, CENTER_Z,
        FIRST_LENGTH, -1, 0,            -0.6*M_PI, 0.1*M_PI,
        SECOND_LENGTH, 1, 0,            -0.25*M_PI, 0.5*M_PI,
        THIRD_LENGTH,  1, THIRD_ANGLE,  -0.5*M_PI, 0.5*M_PI,
        },
    { -CENTER_X, CENTER_Y, CENTER_Z,
        FIRST_LENGTH, -1, 0,            -0.6*M_PI, 0.1*M_PI,
        SECOND_LENGTH, 1, 0,            -0.25*M_PI, 0.5*M_PI,
        THIRD_LENGTH,  1, THIRD_ANGLE,  -0.5*M_PI, 0.5*M_PI,
        },
    { CENTER_X, -CENTER_Y, CENTER_Z, 
        FIRST_LENGTH, -1, 0,            -0.6*M_PI, 0.1*M_PI,
        SECOND_LENGTH, 1, 0,            -0.25*M_PI, 0.5*M_PI,
        THIRD_LENGTH,  1, THIRD_ANGLE,  -0.5*M_PI, 0.5*M_PI,
        },
    { -CENTER_X, -CENTER_Y, CENTER_Z,
        FIRST_LENGTH, -1, 0,            -0.6*M_PI, 0.1*M_PI,
        SECOND_LENGTH, 1, 0,            -0.25*M_PI, 0.5*M_PI,
        THIRD_LENGTH,  1, THIRD_ANGLE,  -0.5*M_PI, 0.5*M_PI,
        },
};


void get_leg_params(legparams &op) {
    op.center_x = CENTER_X;
    op.center_y = CENTER_Y;
    op.center_z = CENTER_Z;
    op.first_length = FIRST_LENGTH;
    op.second_length = SECOND_LENGTH;
    op.third_length = THIRD_LENGTH;
}

std::string solve_error;

//  Solve the leg to point to the given position.
//  If there is no acceptable solution, false is returned, 
//  and some pose approximating the general direction intended 
//  is returned as the "solution."
//  Coordinates are in absolute body-relative coordinates.
//  X -> right
//  Y -> forward
//  Z -> up
bool solve_leg(leginfo const &leg, float x, float y, float z, legpose &op) {

    const float hpi = M_PI * 0.5;
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

    if (dx < 0 && dy < FIRST_LENGTH) {
        //  don't allow poses that turn the legs inside the body
        dx = 0;
        ret = false;
        solve_error = "pose cannot go inside body sideways";
    }
    if (dz > 0) {
        //  don't allow poses that lift legs above the hips
        dz = 0;
        ret = false;
        solve_error = "pose cannot lift legs above hips";
    }
    if (fabsf(dx) < 1e-3 && fabsf(dy) < 1e-3) {
        //  make sure it doesn't go divide-by-zero
        dx = 1e-3;
    }
    //  dx, dy, dz, are relative to the hip joint
    //  orient the thigh in the direction of the target point
    float xylen = sqrtf(dx*dx + dy*dy);
    float xynorm = 1.0f / xylen;
    float ang0 = atan2f(dy, dx) - (float)(0.5*M_PI);    //  0 == front
    //  Calculate distance from the oriented point.
    //  The fixed-length thigh bone points in the direction of the desired point
    float hdx = dx - dx * xynorm * leg.x0;
    float hdy = dy - dy * xynorm * leg.x0;
    //  hdx, hdy are relative to the knee joint
    float maxreach = leg.x1 + leg.l2;
    float minreach = fabsf(leg.x1 - leg.l2);
    float distance = sqrtf(hdx*hdx + hdy*hdy + dz*dz);
    if (distance < 1e-3) {
        dz = -1e-3;
    }
    if (distance < minreach) {
        ret = false;
        solve_error = "Leg cannot reach close to knee.";
        //  fix up by moving leg downwards
        dz = -minreach;
        distance = sqrtf(hdx*hdx + hdy*hdy + dz*dz);
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
    assert(distance >= minreach && distance <= maxreach);

    //  Now I have a triangle, from knee joint, to ankle joint, to foot contact point.
    //  I know the length of each of the sides; now I want to know the coordinates of 
    //  the sides so I can calculate the angles.
    //  a == distance, b == x1, c == l2
    //  A = angle at ankle, B = angle at foot, C = angle at knee (from foot, to ankle)
    //  Cosine rule: a2 = b2 + c2 - 2bc cos A.
    float cosA = (leg.x1*leg.x1 + leg.l2*leg.l2 - distance*distance) / (2 * leg.x1 * leg.l2);
    assert(cosA <= 1.0f);
    float ang2 = acosf(cosA) - hpi;
    float cosC = (leg.x1*leg.x1 + distance*distance - leg.l2*leg.l2) / (2 * leg.x1 * distance);
    assert(cosC <= 1.0f);
    float angC = acosf(cosC);
    //  Now, I actually know the right triangle from point of contact, to below knee, to knee
    float xydist = sqrtf(hdx*hdx + hdy*hdy);
    float Bprime = hpi - atan2f(xydist, -dz);
    float ang1 = angC - Bprime;
    if (ang1 > hpi && z < 0) {
        assert(angC < M_PI);
        ang1 += (angC - M_PI) * 2;
        assert(ang1 <= hpi);
    }

    if (ang0 < leg.min0) {
        ang0 = leg.min0;
        ret = false;
        solve_error = "hip turn below minimum";
    }
    if (ang0 > leg.max0) {
        ang0 = leg.max0;
        ret = false;
        solve_error = "hip turn above maximum";
    }
    if (ang1 < leg.min1) {
        ang1 = leg.min1;
        ret = false;
        solve_error = "knee turn below minimum";
    }
    if (ang1 > leg.max1) {
        ang1 = leg.max1;
        ret = false;
        solve_error = "knee turn above maximum";
    }
    if (ang2 < leg.min2) {
        ang2 = leg.min2;
        ret = false;
        solve_error = "ankle turn below minimum";
    }
    if (ang2 > leg.max2) {
        ang2 = leg.max2;
        ret = false;
        solve_error = "angle turn above maximum";
    }

    ang0 = (ang0 + leg.delta0) * (flip ? -1 : 1) * leg.direction0;
    ang1 = (ang1 + leg.delta1) * (flip ? -1 : 1) * leg.direction1;
    ang2 = (ang2 + leg.delta2) * (flip ? -1 : 1) * leg.direction2;

    //  Now, orient the output solution based on the orientation of the servos
    op.a = 2048 + ang0 * 2048 / M_PI;  //  assume ori right/outwards
    op.b = 2048 + ang1 * 2048 / M_PI;  //  assume ori right/outwards
    op.c = 2048 + ang2 * 2048 / M_PI;  //  assume ori down/outwards (perpendicular to thigh bone)

    assert(op.a >= 0 && op.a <= 4095);
    assert(op.b >= 0 && op.b <= 4095);
    assert(op.c >= 0 && op.c <= 4095);

    return ret;
}



