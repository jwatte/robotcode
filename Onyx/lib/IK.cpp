#include "IK.h"
#include "ServoSet.h"
#include <math.h>
#include <boost/lexical_cast.hpp>
#include "Transform.h"

#define CENTER_X 45.0f
#define CENTER_Y 62.0f
#define CENTER_Z 40.0f

#define FIRST_LENGTH 60.0f
#define SECOND_LENGTH 80.0f
#define THIRD_LENGTH 161.0f
#define THIRD_HORIZONTAL 50.0f

#define THIRD_ANGLE (asinf(THIRD_HORIZONTAL/THIRD_LENGTH))

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

    solve_error = "";
    if (dx < 0 && dy < FIRST_LENGTH) {
        //  don't allow poses that turn the legs inside the body
        dx = 0;
        ret = false;
        solve_error += "pose cannot go inside body sideways\n";
    }
    if (dz > 0) {
        //  don't allow poses that lift legs above the hips
        dz = 0;
        ret = false;
        solve_error += "pose cannot lift legs above hips\n";
    }
    if (fabsf(dx) < 1e-3 && fabsf(dy) < 1e-3) {
        //  make sure it doesn't go divide-by-zero
        dx = 1e-3;
    }
    //  dx, dy, dz, are relative to the hip joint
    //  orient the thigh in the direction of the target point
    float ang0 = atan2f(dy, dx) - (float)(0.5*M_PI);    //  0 == front
    //  Calculate distance from the oriented point.
    //  The fixed-length thigh bone points in the direction of the desired point
    float sin_ang0 = sinf(ang0);
    float cos_ang0 = cosf(ang0);
    float hdx = dx + sin_ang0 * leg.x0;   //  because 0 == front
    float hdy = dy - cos_ang0 * leg.x0;
    //  hdx, hdy are relative to the knee joint
    float maxreach = leg.x1 + leg.l2;
    float minreach = fabsf(leg.x1 - leg.l2);
    float distance = sqrtf(hdx*hdx + hdy*hdy + dz*dz);
    if (distance < 1e-3) {
        dz = -1e-3;
    }
    if (distance < minreach) {
        ret = false;
        std::stringstream ss;
        ss << "distance=" << distance << ", minreach=" << minreach << ", leg too close to knee.\n";
        solve_error += ss.str();
        //  fix up by moving leg downwards
        dz = -minreach;
        distance = sqrtf(hdx*hdx + hdy*hdy + dz*dz);
    }
    if (distance > maxreach) {
        ret = false;
        std::stringstream ss;
        ss << "distance=" << distance << ", minreach=" << minreach << ", leg too far from body.\n";
        solve_error += ss.str();
        //  fix up by moving closer to body
        hdx *= maxreach / distance * 0.9999;
        hdy *= maxreach / distance * 0.9999;
        dz *= maxreach / distance * 0.9999;
        distance = sqrtf(hdx*hdx + hdy*hdy + dz*dz);
    }
    assert(distance >= minreach && distance <= maxreach);

    //  Two-bone IK to find the angles of the knee and ankle joints
    //  First, split the triangle in two right-angle triangles, where 
    //  the base is distance from knee to tip.
    //  Knee-to-tip is split into two segments "a" and "b" at the 
    //  intersection of the perpendicular line "c" from base to ankle.
    //  The "b" segment may have negative length.
    //
    //  c2+a2 = third2
    //  c2+b2 = second2
    //  a+b = distance
    //  b = distance-a
    //  c2+(distance-a)2 = second2
    //  a2-(distance-a)2 = third2-second2
    //  a2-distance2+2adistance-a2 = third2-second2
    //  2adistance-distance2 = third2-second2
    //  a = (third2-second2+distance2)/2distance

    float len_a = (leg.l2*leg.l2-leg.x1*leg.x1+distance*distance)/(2*distance);
    float len_b = distance-len_a;
    assert(leg.l2 >= len_a);
    float len_c = sqrtf(leg.l2*leg.l2-len_a*len_a);

    //  alpha = angle from hip plane to tip
    //  delta = angle from shin to hip/tip
    //  beta = angle from hip plane to shin (which we seek)
    float hdist = sqrtf(hdx*hdx + hdy*hdy);
    float alpha = atan2f(-dz, hdist);
    float delta = atan2f(len_c, len_b);
    float beta = delta - alpha;
    float ang1 = beta;

    //  gamma = angle between "c" and foot
    float gamma = atan2f(len_a, len_c);
    float ang2 = hpi - delta + gamma;
    ang2 = ang2 - hpi;

    std::cerr << "len_a=" << len_a << ", len_b=" << len_b << ", len_c=" <<
        len_c << ", hdist=" << hdist << ", alpha=" << alpha << ", beta=" << 
        beta << ", delta=" << delta << ", gamma=" << gamma << ", ang0=" << 
        ang0 << ", ang1=" << ang1 << ", ang2=" << ang2 << std::endl;

    ang0 = ang0 - leg.delta0;
    if (ang0 < leg.min0) {
        ang0 = leg.min0;
        ret = false;
        solve_error += "hip turn below minimum\n";
    }
    if (ang0 > leg.max0) {
        ang0 = leg.max0;
        ret = false;
        solve_error += "hip turn above maximum\n";
    }
    ang1 = ang1 - leg.delta1;
    if (ang1 < leg.min1) {
        ang1 = leg.min1;
        ret = false;
        solve_error += "knee turn below minimum\n";
    }
    if (ang1 > leg.max1) {
        ang1 = leg.max1;
        ret = false;
        solve_error += "knee turn above maximum\n";
    }
    ang2 = ang2 - leg.delta2;
    if (ang2 < leg.min2) {
        ang2 = leg.min2;
        ret = false;
        solve_error += "ankle turn below minimum\n";
    }
    if (ang2 > leg.max2) {
        ang2 = leg.max2;
        ret = false;
        solve_error += "angle turn above maximum\n";
    }

    ang0 = ang0 * (flip ? -1 : 1) * leg.direction0;
    ang1 = ang1 * (flip ? -1 : 1) * leg.direction1;
    ang2 = ang2 * (flip ? -1 : 1) * leg.direction2;

    //  Now, orient the output solution based on the orientation of the servos
    op.a = 2048 + ang0 * 2048 / M_PI;  //  assume ori right/outwards
    op.b = 2048 + ang1 * 2048 / M_PI;  //  assume ori right/outwards
    op.c = 2048 + ang2 * 2048 / M_PI;  //  assume ori down/outwards (perpendicular to thigh bone)

    assert(op.a >= 0 && op.a <= 4095);
    assert(op.b >= 0 && op.b <= 4095);
    assert(op.c >= 0 && op.c <= 4095);

    return ret;
}


void forward_leg(leginfo const &leg, legpose const &lp, float &ox, float &oy, float &oz)
{
    float flip = 1;
    if (leg.cx < 0) {
        flip = flip * -1;
    }
    if (leg.cy < 0) {
        flip = flip * -1;
    }
    /*
    Transform t(
        Translate(leg.cx, leg.cy, leg.cz) *
        Rotate((lp.a - 2048)*M_PI/2048 + leg.delta0, 0, leg.direction0 * flip, 0) *
        Translate(0, leg.x0, 0) *
        Rotate((lp.b - 2048)*M_PI/2048 + leg.delta1, leg.direction1 * flip, 0, 0) *
        Translate(0, leg.x1, 0) *
        Rotate((lp.c - 2048)*M_PI/2048 + leg.delta2, leg.direction2 * flip, 0, 0) *
        Translate(0, 0, -leg.l2)
        );
    vec4 o(t * vec4(0, 0, 0, 1));
    ox = o.v[0];
    oy = o.v[1];
    oz = o.v[2];
     */
    vec4 r(0, 0, 0, 1);
    std::cerr << " a. " << r << std::endl;
    r = Translate(0, 0, -leg.l2) * r;
    std::cerr << "tb. " << r << std::endl;
    r = Rotate((lp.c - 2048)*M_PI/2048 + leg.delta2, leg.direction2 * flip, 0, 0) * r;
    std::cerr << "rc. " << r << std::endl;
    r = Translate(0, leg.x1, 0) * r;
    std::cerr << "td. " << r << std::endl;
    r = Rotate((lp.b - 2048)*M_PI/2048 + leg.delta1, leg.direction1 * flip, 0, 0) * r;
    std::cerr << "re. " << r << std::endl;
    r = Translate(0, leg.x0, 0) * r;
    std::cerr << "tf. " << r << std::endl;
    r = Rotate((lp.a - 2048)*M_PI/2048 + leg.delta0, 0, 0, leg.direction0 * flip) * r;
    std::cerr << "rg. " << r << std::endl;
    r = Translate(leg.cx, leg.cy, leg.cz) * r;
    std::cerr << "th. " << r << std::endl;
    ox = r.x;
    oy = r.y;
    oz = r.z;
}


