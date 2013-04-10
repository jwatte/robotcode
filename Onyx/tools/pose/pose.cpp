
#include "itime.h"
#include "istatus.h"
#include "util.h"
#include "Interpolator.h"

#include <assert.h>

#include "USBLink.h"
#include "ServoSet.h"
#include "IK.h"
#include "util.h"
#include "Camera.h"
#include "Settings.h"
#include "Image.h"
#include <iostream>
#include <sstream>
#include <time.h>
#include <string.h>


bool REAL_USB = true;

unsigned short MAX_TORQUE = 900;
int args[3] = { 0, 0, 0 };
int argp = 0;


static ITime *itime;
static IStatus *istatus;

static unsigned char battery;
legparams lparam;

struct initinfo {
    unsigned short id;
    unsigned short center;
};
static const initinfo init[] = {
    { 1, 2048+768 },
    { 2, 2048-256 },
    { 3, 2048-256 },
    { 4, 2048-768 },
    { 5, 2048+256 },
    { 6, 2048+256 },
    { 7, 2048-768 },
    { 8, 2048+256 },
    { 9, 2048+256 },
    { 10, 2048+768 },
    { 11, 2048-256 },
    { 12, 2048-256 },
    { 13, 2048 },
    { 14, 2048 },
};

static unsigned char nst = 0;

//  slew rates are amount of change per second
#define SPEED_SLEW 2.5f
#define HEIGHT_SLEW 50.0f

const float lift = 40;
const float height_above_ground = 0;

float ctl_trot = 1.5f;
float ctl_speed = 0;
float ctl_turn = 0;
float ctl_strafe = 0;
float ctl_heading = 0;
float ctl_elevation = 0;
unsigned char ctl_pose = 3;

legpose last_pose[4];

const float STRAFE_SIZE = 40.0f;
const float STEP_SIZE = 60.0f;
const float CENTER_XPOS = 120.0f;
const float CENTER_YPOS = 115.0f;
const float CENTER_ZPOS = -85;

void poseleg_pos(ServoSet &ss, int leg, float xpos, float ypos, float zpos) {
    legpose lp;
    if (!solve_leg(legs[leg], xpos, ypos, zpos, lp)) {
        std::stringstream sst;
        sst << "Could not solve leg: " << leg <<
            " xpos " << xpos << " ypos " << ypos
            << " zpos " << zpos;
        static std::string mstr[4];
        mstr[leg] = sst.str();
        istatus->error(mstr[leg].c_str());
    }
    last_pose[leg] = lp;
    ss.id(leg * 3 + 1).set_goal_position(lp.a);
    ss.id(leg * 3 + 2).set_goal_position(lp.b);
    ss.id(leg * 3 + 3).set_goal_position(lp.c);
}

void poseleg_delta(ServoSet &ss, int leg, float dx, float dy, float dz) {
    float xpos = CENTER_XPOS; // lparam.center_x + lparam.first_length + lparam.second_length;
    float ypos = CENTER_YPOS; // lparam.center_y + lparam.first_length;
    float zpos = CENTER_ZPOS;
    if (leg & 1) {
        xpos = -xpos;
    }
    if (leg & 2) {
        ypos = -ypos;
    }
    xpos += dx;
    ypos += dy;
    zpos += dz;
    poseleg_pos(ss, leg, xpos, ypos, zpos);
}

void poseleg(ServoSet &ss, int leg, float step, float speed, float strafe, float deltaPose) {
    float dx = 0, dy = 0, dz = 0;
    if (step < 50) {    //  front-to-back
        float dd = 1.0f - step / 25.0f;
        dx = STRAFE_SIZE * dd * strafe;
        dy = STEP_SIZE * dd * speed;
        dz = 0;
    }
    else {  //  lifted, back-to-front
        float dd = (step - 75) / 25.0f;
        dx = STRAFE_SIZE * dd * strafe;
        dy = STEP_SIZE * dd * speed;
        dz = sinf(M_PI * (dd + 1) / 2) * lift;
        if (std::max(fabsf(speed), fabsf(strafe)) < 0.1) {
            dz = dz * 10 * fabsf(speed);
        }
    }
    poseleg_delta(ss, leg, dx, dy, dz - deltaPose);
}

void poselegs(ServoSet &ss, float step, float speed, float turn, float strafe, float deltaPose) {
    float step50 = step + 50;
    if (step50 >= 100) {
        step50 -= 100;
    }
    poseleg(ss, 0, step, cap(speed + turn), strafe, deltaPose);
    poseleg(ss, 1, step50, cap(speed - turn), strafe, deltaPose);
    poseleg(ss, 2, step50, cap(speed + turn), strafe, deltaPose);
    poseleg(ss, 3, step, cap(speed - turn), strafe, deltaPose);
}


void usb_thread_fn() {
    sched_param parm = { .sched_priority = 25 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "USBLink::thread_fn(): pthread_setschedparam(): " << err << std::endl;
    }
    get_leg_params(lparam);
    ServoSet ss(REAL_USB, boost::shared_ptr<Logger>());
    for (size_t i = 0; i < sizeof(init)/sizeof(init[0]); ++i) {
        ss.add_servo(init[i].id, init[i].center);
    }
    ss.set_torque(MAX_TORQUE, 1); //  not quite top torque

    double thetime = 0, prevtime = 0, intime = read_clock();
    double frames = 0;
    float phase = 0;
    while (true) {
        thetime = read_clock();
        frames = frames + 1;
        if (thetime - intime > 10) {
            fprintf(stderr, "usb fps: %.1f; battery: %.1f\n", frames / (thetime - intime), battery/10.0f);
            frames = 0;
            intime = thetime;
        }
        float dt = thetime - prevtime;

        if (dt >= 0.008) {
            phase += dt;
            float scale = sinf(fmod(phase, 2 * M_PI));
            poseleg_delta(ss, 0, args[0] * scale, args[1], args[2]);
            poseleg_delta(ss, 1, args[0] * scale, args[1], args[2]);
            poseleg_delta(ss, 2, args[0] * scale, args[1], args[2]);
            poseleg_delta(ss, 3, args[0], args[1], args[2]);
            prevtime = thetime;
        }
        else {
            usleep(3000);
        }

        ss.step();
        battery = ss.battery();

        if (ss.queue_depth() > 30) {
            istatus->error("Servo queue overflow -- flushing.");
            int n = 100;
            while (ss.queue_depth() > 0 && n > 0) {
                --n;
                ss.step();
            }
        }

        unsigned char status[32];
        unsigned char bst = ss.get_status(status, 32);
        unsigned char st = 0;
        for (unsigned char si = 0; si != bst; ++si) {
            st = st | status[si];
        }
        if (st != nst) {
            std::cerr << "status: " << hexnum(st);
            nst = st;
            for (unsigned char si = 0; si != bst; ++si) {
                if (status[si]) {
                    std::cerr << " " << (int)si << ":" << hexnum(status[si]);
                }
            }
            std::cerr << std::endl;
        }
    }
}

int main(int argc, char const *argv[]) {
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--fakeusb")) {
            REAL_USB = false;
        }
        else if (!strcmp(argv[1], "--maxtorque")) {
            if (argv[2] == nullptr) {
                goto usage;
            }
            MAX_TORQUE = atoi(argv[2]);
            if (MAX_TORQUE < 1 || MAX_TORQUE > 1023) {
                goto usage;
            }
            ++argv;
            --argc;
        }
        else if (argp < 3) {
            args[argp] = atoi(argv[i]);
            if (args[argp] == 0 && strcmp(argv[i], "0")) {
                fprintf(stderr, "delta must be a number; got '%s'\n", argv[i]);
                goto usage;
            }
            if (args[argp] < -100 || args[argp] > 100) {
                fprintf(stderr, "delta is out of range [-100,100]; got %d\n", args[argp]);
                goto usage;
            }
        }
        else {
usage:
            fprintf(stderr, "usage: robot [--fakeusb] [--maxtorque 1023] [dx [dy [dz]]]\n");
            exit(1);
        }
    }
    itime = newclock();
    istatus = mkstatus(itime, true);

    boost::shared_ptr<boost::thread> usb_thread(new boost::thread(boost::bind(usb_thread_fn)));

    boost::shared_ptr<Settings> settings(Settings::load("onyx.json"));

    double thetime = 0, intime = read_clock();
    double frames = 0;
    while (true) {

        thetime = read_clock();
        frames = frames + 1;
        if (thetime - intime > (REAL_USB ? 20 : 2)) {
            fprintf(stderr, "main fps: %.1f  battery: %.1f\n", frames / (thetime - intime),
                (float)battery / 10.0);
            frames = 0;
            intime = thetime;
            if (!REAL_USB) {
                fprintf(stderr, "1:%d 2:%d 3:%d  4:%d 5:%d 6:%d  7:%d 8:%d 9:%d  10:%d 11:%d 12:%d\n",
                    last_pose[0].a, last_pose[0].b, last_pose[0].c,
                    last_pose[1].a, last_pose[1].b, last_pose[1].c,
                    last_pose[2].a, last_pose[2].b, last_pose[2].c,
                    last_pose[3].a, last_pose[3].b, last_pose[3].c);
            }
        }
        usleep(10000);
    }
    return 0;
}

