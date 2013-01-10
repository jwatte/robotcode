
#include "itime.h"
#include "istatus.h"
#include "inetwork.h"
#include "util.h"
#include "protocol.h"
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>


//  never run faster than 40 Hz?
#define MIN_LOOP_TIME 0.025

unsigned short port = 6969;

ITime *itime;
IStatus *istatus;
ISockets *isocks;
INetwork *inet;
IPacketizer *ipacketizer;

bool has_robot = false;
double last_status_time = 0;

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
    istatus->message(std::string("Joystick ") + joy_name + " "
        + boost::lexical_cast<std::string>(joy_num_axes) + " axes "
        + boost::lexical_cast<std::string>(joy_num_buttons) + " buttons.");
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


void do_info(P_Info const *info) {
    istatus->message(std::string("Found robot ") + info->name + " pilot " + info->pilot + ".");
    P_Connect conn;
    memset(&conn, 0, sizeof(conn));
    safecpy(conn.pilot, "Some Dude");
    ipacketizer->respond(C2R_Connect, sizeof(conn), &conn);
}

void do_status(P_Status const *status) {
    if (!has_robot) {
        inet->lock_address(5);
        has_robot = true;
    }
    last_status_time = itime->now();
    istatus->message(std::string("Status: hits=") + boost::lexical_cast<std::string>((int)status->hits)
        + " status=" + hexnum(status->status) + " message=" + status->message);
}

void do_videoframe(P_VideoFrame const *videoframe) {
}

void dispatch(unsigned char type, size_t size, void const *data) {
    switch (type) {
    case R2C_Info:
        if (size < sizeof(P_Info)) {
            istatus->message("Bad packet size " + boost::lexical_cast<std::string>(size) + " for R2C_Info.");
        }
        else {
            do_info((P_Info const *)data);
        }
        break;
    case R2C_Status:
        if (size < sizeof(P_Status)) {
            istatus->message("Bad packet size " + boost::lexical_cast<std::string>(size) + " for R2C_Status.");
        }
        else {
            do_status((P_Status const *)data);
        }
        break;
    case R2C_VideoFrame:
        if (size < sizeof(P_VideoFrame)) {
            istatus->message("Bad packet size " + boost::lexical_cast<std::string>(size) + " for R2C_VideoFrame.");
        }
        else {
            do_videoframe((P_VideoFrame const *)data);
        }
        break;
    default:
        //  just ignore it
        break;
    }
}

void scan_for_robots() {
    P_Discover pd;
    ipacketizer->broadcast(C2R_Discover, sizeof(pd), &pd);
}

int main(int argc, char const *argv[]) {

    itime = newclock();
    istatus = mkstatus(itime, true);
    isocks = mksocks(port, istatus);
    inet = scan(isocks, itime, istatus);
    ipacketizer = packetize(inet, istatus);

    joyopen();

    double then = itime->now();
    double bc = 0;
    while (true) {
        double now = itime->now();
        double dt = now - then;
        if (dt < MIN_LOOP_TIME) {
            itime->sleep(MIN_LOOP_TIME - dt);
            now = itime->now();
            dt = now - then;
        }
        then = now;
        joystep();
        if (has_robot && now > last_status_time + 5) {
            istatus->error("Lost connection to robot.");
            has_robot = false;
            inet->unlock_address();
        }
        if (has_robot) {
            P_SetInput seti;
            memset(&seti, 0, sizeof(seti));
            seti.trot = trotvals[joytrotix];
            seti.speed = joyspeed;
            seti.turn = joyturn;
            ipacketizer->respond(C2R_SetInput, sizeof(seti), &seti);
        }
        if (!has_robot && now > bc) {
            scan_for_robots();
            bc = now + 1;
        }
        ipacketizer->step();
        unsigned char type = 0;
        size_t size = 0;
        void const *data = 0;
        while (ipacketizer->receive(type, size, data)) {
            dispatch(type, size, data);
        }
    }

    return 1;
}
