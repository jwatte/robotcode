
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
        if (speed < 0.1) {
            dz = dz * 10 * speed;
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
        std::cerr << "Could not solve leg: " << leg << " step " << step << std::endl;
        exit(1);
    }
    ss.id(leg * 3 + 1).set_goal_position(lp.a);
    ss.id(leg * 3 + 2).set_goal_position(lp.b);
    ss.id(leg * 3 + 3).set_goal_position(lp.c);
}

void poselegs(ServoSet &ss, float step, float speed) {
    float step50 = step + 50;
    if (step50 >= 100) {
        step50 -= 100;
    }
    poseleg(ss, 0, step, speed);
    poseleg(ss, 1, step50, speed);
    poseleg(ss, 2, step50, speed);
    poseleg(ss, 3, step, speed);
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
    float speed = 0.1;
    float trot = 1.5;
    while (true) {
        thetime = read_clock();
        float use_trot = trot;
        float use_speed = speed;
        if (ss.torque_pending()) {
            use_speed = 0;
            use_trot = 0;
        }
        if (thetime - prevtime >= 0.01) {
            if (prevtime != 0) {
                step += (thetime - prevtime) * 100 * use_trot;
                prevtime += 0.01;
            }
            else {
                prevtime = thetime;
            }
            //  running speed -- one full cycle per 1000 ms
            while (step >= 100) {
                step -= 100;
            }
            while (step < 0) {
                step += 100;
            }
        }
        poselegs(ss, step, use_speed);
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

