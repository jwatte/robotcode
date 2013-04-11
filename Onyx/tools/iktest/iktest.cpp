
#include "itime.h"
#include "inetwork.h"
#include "istatus.h"
#include "protocol.h"
#include "util.h"
#include "Interpolator.h"
#include "logger.h"

#include <assert.h>
#include <ctype.h>

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



#define MAX_SERVO_COUNT 16

bool REAL_USB = true;

static double const LOCK_ADDRESS_TIME = 5.0;
static double const STEP_DURATION = 0.008;
unsigned short MAX_TORQUE = 900;

static ITime *itime;
static IStatus *istatus;
static unsigned char battery;
static unsigned char maxtemp;
static volatile bool running = true;

legparams lparam;

struct initinfo {
    unsigned short id;
    unsigned short center;
};
static const initinfo init[] = {
    { 1, 2048+256 },
    { 2, 2048-256 },
    { 3, 2048-256 },
    { 4, 2048-256 },
    { 5, 2048+256 },
    { 6, 2048+256 },
    { 7, 2048-256 },
    { 8, 2048+256 },
    { 9, 2048+256 },
    { 10, 2048+256 },
    { 11, 2048-256 },
    { 12, 2048-256 },
    { 13, 2048 },
    { 14, 2048 },
};

static unsigned char nst = 0;

//  slew rates are amount of change per second
#define SPEED_SLEW 2.5f
#define HEIGHT_SLEW 50.0f

const float stride = 200;
const float lift = 40;

legpose last_pose[4];

//  center of a "standing" pose is where?
const float CENTER_XPOS = 140.0f;
const float CENTER_YPOS = 155.0f;
const float CENTER_ZPOS = -0.0f;

struct legposition {
    float dx;
    float dy;
    float dz;
};
legposition legpos[4];

void poseleg(int leg, float dx, float dy, float dz) {
    float xpos = CENTER_XPOS;
    float ypos = CENTER_YPOS;
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
    legpose lp;
    if (!solve_leg(legs[leg], xpos, ypos, zpos, lp)) {
        std::stringstream sst;
        sst << "Could not solve leg: " << leg 
            << " xpos " << xpos << " ypos " << ypos << " zpos " << zpos;
        static std::string mstr[4];
        mstr[leg] = sst.str();
        istatus->error(mstr[leg].c_str());
    }
    last_pose[leg] = lp;
    fprintf(stderr, "%f,%f,%f => %d,%d,%d\n", dx, dy, dz, lp.a, lp.b, lp.c);
}

void setlegpose(ServoSet &ss, int leg, legpose const &pose) {
    ss.id(leg * 3 + 1).set_goal_position(pose.a);
    ss.id(leg * 3 + 2).set_goal_position(pose.b);
    ss.id(leg * 3 + 3).set_goal_position(pose.c);
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
    unsigned short mt = MAX_TORQUE;

    double thetime = 0, prevtime = 0, intime = read_clock();
    SlewRateInterpolator<float> i_speed(0, SPEED_SLEW, 1, intime);
    SlewRateInterpolator<float> i_strafe(0, SPEED_SLEW, 1, intime);
    SlewRateInterpolator<float> i_turn(0, SPEED_SLEW, 1, intime);
    SlewRateInterpolator<float> i_height(0, HEIGHT_SLEW, 1, intime);
    double frames = 0;

    while (running) {
        thetime = read_clock();
        frames = frames + 1;
        if (thetime - intime > 20) {
            fprintf(stderr, "\nusb fps: %.1f  battery: %.1f  maxtemp: %d\n", frames / (thetime - intime),
                (float)battery / 10.0, maxtemp);
            if (!REAL_USB) {
                fprintf(stderr, "\n1:%d 2:%d 3:%d  4:%d 5:%d 6:%d  7:%d 8:%d 9:%d  10:%d 11:%d 12:%d\n",
                    last_pose[0].a, last_pose[0].b, last_pose[0].c,
                    last_pose[1].a, last_pose[1].b, last_pose[1].c,
                    last_pose[2].a, last_pose[2].b, last_pose[2].c,
                    last_pose[3].a, last_pose[3].b, last_pose[3].c);
            }
            frames = 0;
            intime = thetime;
        }
        float dt = thetime - prevtime;

        if (dt >= STEP_DURATION) {
            setlegpose(ss, 0, last_pose[0]);
            setlegpose(ss, 1, last_pose[1]);
            setlegpose(ss, 2, last_pose[2]);
            setlegpose(ss, 3, last_pose[3]);
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
        unsigned char status[MAX_SERVO_COUNT];
        unsigned char bst = ss.get_status(status, MAX_SERVO_COUNT);
        unsigned char st = 0;
        for (unsigned char si = 0; si != bst; ++si) {
            st = st | status[si];
        }
        if (st != nst) {
            std::stringstream strstr;
            strstr << "status: " << hexnum(st);
            nst = st;
            for (unsigned char si = 0; si != bst; ++si) {
                if (status[si]) {
                    strstr << " " << (int)si << ":" << hexnum(status[si]);
                }
            }
            istatus->error(strstr.str());
        }

        ss.slice_reg1(REG_PRESENT_TEMPERATURE, status, MAX_SERVO_COUNT);
        for (size_t i = 0; i != MAX_SERVO_COUNT; ++i) {
            if (status[i] > 75) {
                std::stringstream strstr;
                strstr << "Temp for servo " << (int)i << " is " << (int)status[i];
                istatus->error(strstr.str());
                system("bld/obj/off");
                exit(1);
            }
            if (status[i] > maxtemp) {
                maxtemp = status[i];
            }
        }

        unsigned short status2[MAX_SERVO_COUNT] = { 0 };
        ss.slice_reg2(REG_CURRENT, status2, MAX_SERVO_COUNT);

        unsigned short rd = MAX_TORQUE;
        if (rd != mt) {
            ss.set_torque(rd, 1);
            mt = rd;
        }
        usleep(1000);
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
            if (MAX_TORQUE < 0 || MAX_TORQUE > 1023) {
                goto usage;
            }
            ++argv;
            --argc;
        }
        else {
usage:
            fprintf(stderr, "usage: robot [--fakeusb] [--maxtorque 1023]\n");
            exit(1);
        }
    }
    itime = newclock();
    IStatus *chain = mkstatus(itime, true);
    istatus = chain;
    boost::shared_ptr<boost::thread> usb_thread(new boost::thread(boost::bind(usb_thread_fn)));
    boost::shared_ptr<Settings> settings(Settings::load("onyx.json"));
    last_pose[0].a = init[0].center;
    last_pose[0].b = init[1].center;
    last_pose[0].c = init[2].center;
    last_pose[1].a = init[3].center;
    last_pose[1].b = init[4].center;
    last_pose[1].c = init[5].center;
    last_pose[2].a = init[6].center;
    last_pose[2].b = init[7].center;
    last_pose[2].c = init[8].center;
    last_pose[3].a = init[9].center;
    last_pose[3].b = init[10].center;
    last_pose[3].c = init[11].center;

    while (true) {
        fprintf(stderr, "cmd> ");
        fflush(stdout);
        char line[256] = { 0 };
        fgets(line, 255, stdin);
        if (!line[0]) {
            break;
        }
        char *end = &line[strlen(line)];
        while (end > line && isspace(end[-1])) {
            end[-1] = 0;
            --end;
        }
        if (!strcmp(line, "quit")) {
            break;
        }
        if (!strcmp(line, "help") || !strcmp(line, "?")) {
            fprintf(stderr, "commands:\n");
            fprintf(stderr, "help | ?               print this text.\n");
            fprintf(stderr, "quit                   exit this program.\n");
            fprintf(stderr, "off                    turn off all torque.\n");
            fprintf(stderr, "torque val             turn on all torque.\n");
            fprintf(stderr, "leg # dx dy dz         pose a particular leg\n");
            continue;
        }
        std::stringstream sst;
        sst.str(line);
        std::string cmd;
        sst >> cmd;
        if (cmd == "leg") {
            int lnum = 0;
            float dx = 0;
            float dy = 0;
            float dz = 0;
            sst >> lnum >> dx >> dy >> dz;
            if (lnum < 0 || lnum > 3) {
                fprintf(stderr, "Bad leg num: %d\n", lnum);
                continue;
            }
            legpos[lnum].dx = dx;
            legpos[lnum].dy = dy;
            legpos[lnum].dz = dz;
            poseleg(lnum, dx, dy, dz);
            continue;
        }
        if (cmd == "off") {
            MAX_TORQUE = 0;
            continue;
        }
        if (cmd == "torque") {
            int t = 0;
            sst >> t;
            if (t < 0 || t > 1023) {
                fprintf(stderr, "Bad torque value: %d\n", t);
                continue;
            }
            MAX_TORQUE = t;
            continue;
        }
        fprintf(stderr, "Command not recognized: '%s'\n", line);
    }
    running = false;
    usb_thread->join();

    return 0;
}


