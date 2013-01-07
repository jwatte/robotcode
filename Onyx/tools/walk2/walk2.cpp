
#include "ServoSet.h"
#include "IK.h"
#include "util.h"
#include <iostream>
#include <time.h>
#include <string.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>


legparams lparam;

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


#define TROT_UP_BUTTON 0
#define TROT_DOWN_BUTTON 1

#define SPEED_AXIS 1
#define TURN_AXIS 0
#define AIM_X_AXIS 2
#define AIM_Y_AXIS 3
#define MAXJOY 28000.0f

float joyspeed = 0;
float joyturn = 0;
int joytrotix = 15;

float const trotvals[22] = {
    0.1f,       //  0
    0.15f,
    0.2f,
    0.25f,
    0.3f,
    0.375f,     //  5
    0.45f,
    0.525f,
    0.6f,
    0.7f,
    0.8f,       //  10
    0.9f,
    1.0f,
    1.15f,
    1.3f,
    1.5f,       //  15
    1.75f,
    2.0f,
    2.25f,
    2.5f,
    2.75f,
    3.0f        //  20
};



#define SPEED_SLEW 2.0f

const float stride = 200;
const float lift = 40;
const float height_above_ground = 80;


void poseleg(ServoSet &ss, int leg, float step, float speed) {
    float dx = 0, dy = 0, dz = 0;
    if (step < 50) {    //  front-to-back
        dx = 0;
        dy = (100 - 4 * step) * speed;
        dz = 0;
    }
    else {  //  lifted, back-to-front
        dx = 0;
        dy = (step * 4 - 300) * speed;
        dz = sinf((step - 50) * M_PI / 50) * lift;
        if (fabsf(speed) < 0.1) {
            dz = dz * 10 * fabsf(speed);
        }
    }
    float xpos = lparam.center_x + lparam.first_length + lparam.second_length;
    float ypos = lparam.center_y + lparam.first_length;
    float zpos = -height_above_ground;
    if (leg & 1) {
        xpos = -xpos;
    }
    if (leg & 2) {
        ypos = -ypos;
    }
    xpos += dx;
    ypos += dy;
    zpos += dz;
    legpose lp;
    if (!solve_leg(legs[leg], xpos, ypos, zpos, lp)) {
        std::cerr << "Could not solve leg: " << leg << " step " << step
            << " speed " << speed << " xpos " << xpos << " ypos " << ypos
            << " zpos " << zpos << std::endl;
        abort();
    }
    ss.id(leg * 3 + 1).set_goal_position(lp.a);
    ss.id(leg * 3 + 2).set_goal_position(lp.b);
    ss.id(leg * 3 + 3).set_goal_position(lp.c);
}

void poselegs(ServoSet &ss, float step, float speed, float turn) {
    float step50 = step + 50;
    if (step50 >= 100) {
        step50 -= 100;
    }
    poseleg(ss, 0, step, cap(speed + turn));
    poseleg(ss, 1, step50, cap(speed - turn));
    poseleg(ss, 2, step50, cap(speed + turn));
    poseleg(ss, 3, step, cap(speed - turn));
}

int joyfd = -1;
int joy_num_axes = 2;
int joy_num_buttons = 2;
char joy_name[80];

bool joyopen() {
    if (joyfd != -1) {
        return true;
    }
    joyfd = open("/dev/input/js0", O_RDONLY | O_NONBLOCK);
    if (joyfd < 0) {
        perror("/dev/input/js0");
        return false;
    }
    ioctl(joyfd, JSIOCGAXES, &joy_num_axes);
    ioctl(joyfd, JSIOCGBUTTONS, &joy_num_buttons);
    ioctl(joyfd, JSIOCGNAME(80), joy_name);
    std::cerr << "joystick " << joy_name << " " << joy_num_axes << " axes "
        << joy_num_buttons << " buttons." << std::endl;
    return true;
}

void joystep() {
    if (joyfd < 0) {
        return;
    }
    struct js_event js;
    for (int i = 0; i < 20; ++i) {
        if (read(joyfd, &js, sizeof(js)) < 0) {
            break;
        }
        else if (js.type & JS_EVENT_INIT) {
            continue;
        }
        else if (js.type & JS_EVENT_AXIS) {
            if (js.number == SPEED_AXIS) {
                joyspeed = cap(js.value / - MAXJOY);
            }
            else if (js.number == TURN_AXIS) {
                joyturn = cap(js.value / MAXJOY);
            }
            else if (js.number == AIM_X_AXIS) {
            }
            else if (js.number == AIM_Y_AXIS) {
            }
            else {
                std::cerr << "axis " << (int)js.number << " value " << js.value << std::endl;;
            }
        }
        else if (js.type & JS_EVENT_BUTTON) {
            bool on = js.value != 0;
            if (js.number == TROT_UP_BUTTON) {
                if (on) {
                    joytrotix++;
                    if ((size_t)joytrotix >= sizeof(trotvals)/sizeof(trotvals[0])) {
                        joytrotix = (int)sizeof(trotvals)/sizeof(trotvals[0])-1;
                    }
                    std::cerr << "trot " << trotvals[joytrotix] << std::endl;
                }
            }
            else if (js.number == TROT_DOWN_BUTTON) {
                if (on) {
                    if (joytrotix > 0) {
                        joytrotix--;
                        std::cerr << "trot " << trotvals[joytrotix] << std::endl;
                    }
                }
            }
            else {
                std::cerr << "button " << (int)js.number << " value " << js.value << std::endl;
            }
        }
    }
}


int main() {
    joyopen();
    get_leg_params(lparam);
    ServoSet ss;
    for (size_t i = 0; i < sizeof(init)/sizeof(init[0]); ++i) {
        ss.add_servo(init[i].id, init[i].center);
    }
    ss.set_torque(900); //  not quite top torque

    double thetime = 0, prevtime = 0;
    float step = 0;
    float prevspeed = 0;
    while (true) {
        joystep();
        thetime = read_clock();
        float use_trot = trotvals[joytrotix];
        float use_speed = joyspeed;
        float use_turn = joyturn;
        if (ss.torque_pending()) {
            use_speed = 0;
            use_trot = 0;
            use_turn = 0;
        }
        float dt = thetime - prevtime;
        if (dt >= 0.01) {
            //  don't fall more than 0.1 seconds behind, else catch up in one swell foop
            if (dt < 0.1f) {
                step += dt * 100 * use_trot;
                prevtime += 0.01;
            }
            else {
                //  and don't update step!
                prevtime = thetime;
            }
            //  "step" is a measure of the cycle in percent
            while (step >= 100) {
                step -= 100;
            }
            while (step < 0) {
                step += 100;
            }
            if (use_speed > prevspeed) {
                use_speed = std::min(prevspeed + dt * SPEED_SLEW, use_speed);
            }
            else if (use_speed < prevspeed) {
                use_speed = std::max(prevspeed - dt * SPEED_SLEW, use_speed);
            }
            prevspeed = use_speed;
            poselegs(ss, step, use_speed, use_turn);
        }
        ss.step();
        if (ss.queue_depth() > 30) {
            std::cerr << "queue_depth: " << ss.queue_depth() << std::endl;
            while (ss.queue_depth() > 0) {
                ss.step();
            }
        }
        unsigned char status[33];
        unsigned char st = ss.get_status(status, 33);
        static unsigned char nst = 0;
        if (st != nst) {
            std::cerr << "status: 0x" << std::hex << (int)st << std::dec << std::endl;
            nst = st;
        }
        usleep(500);
    }
}

