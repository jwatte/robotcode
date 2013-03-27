
#include "ServoSet.h"
#include "IK.h"
#include "util.h"
#include <iostream>
#include <time.h>
#include <string.h>


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

int main() {
    get_leg_params(lparam);
    ServoSet ss(true, boost::shared_ptr<Logger>());
    for (size_t i = 0; i < sizeof(init)/sizeof(init[0]); ++i) {
        ss.add_servo(init[i].id, init[i].center);
    }
    ss.set_torque(900); //  not quite top torque

    bool prevsolved = true;
    double thetime = 0, prevtime = 0;
    float step = 0;
    float speed = 1;
    while (true) {
        thetime = read_clock();
        float use_speed = speed;
        if (ss.torque_pending()) {
            use_speed = 0;
        }
        if (thetime - prevtime >= 0.01) {
            if (prevtime != 0) {
                step += (thetime - prevtime) * 100 * use_speed;
                prevtime += 0.01;
            }
            else {
                prevtime = thetime;
            }
            //  running speed -- one full cycle per 1000 ms
            while (step >= 100) {
                step -= 100;
            }
            if (step < 0) {
                step += 100;
            }
            //cmd_pose cp[12] = { { 0 } };
            for (int leg = 0; leg < 4; ++leg) {
                double xpos = lparam.center_x + lparam.first_length + lparam.second_length;
                if (leg & 1) {
                    xpos = -xpos;
                }
                double ypos = lparam.center_y + lparam.first_length;
                if (leg & 2) {
                    ypos = -ypos;
                }
                double zpos = -80;
                //  actual walk parameter
                int gait = leg;
                switch (leg) {
                    case 1: gait = 2; break;
                    case 2: gait = 1; break;
                }
                if (step >= gait * 25 && step < (gait + 1) * 25) {
                    zpos += 20;
                    ypos += (step - gait * 25) * 8 - 100;
                }
                else {
                    float phase = 0;
                    if (step < gait * 25) {
                        phase = (step - gait * 25) / 37.5 + 1;
                    }
                    else {
                        phase = (step - (gait + 1) * 25) / 37.5 - 1;
                    }
                    if (phase < -1) {
                        phase += 2;
                    }
                    if (phase > 1) {
                        phase -= 2;
                    }
                    ypos -= phase * 100;
                }
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
                ss.id(leg*3+1).set_goal_position(oot.a);
                ss.id(leg*3+2).set_goal_position(oot.b);
                ss.id(leg*3+3).set_goal_position(oot.c);
                /*
                cp[3*leg+0].id = 3*leg+1;
                cp[3*leg+0].pose = oot.a;
                cp[3*leg+1].id = 3*leg+2;
                cp[3*leg+1].pose = oot.b;
                cp[3*leg+2].id = 3*leg+3;
                cp[3*leg+2].pose = oot.c;
                */
            }
            //ss.lerp_pose(10, cp, 12);
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

