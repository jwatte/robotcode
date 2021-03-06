
#include "itime.h"
#include "istatus.h"
#include "inetwork.h"
#include "util.h"
#include "protocol.h"
#include "gui.h"
#include "Image.h"
#include "mwscore.h"
#include "Settings.h"
#include <iostream>
#include <boost/lexical_cast.hpp>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/joystick.h>


//  never run faster than 125 Hz?
#define MIN_LOOP_TIME 0.008
#define VIDEO_REQUEST_INTERVAL 0.25
#define VIDEO_REQUEST_WIDTH 1280
#define VIDEO_REQUEST_HEIGHT 720
#define Q_CHECK_INTERVAL 0.25

unsigned short port = 6969;

ITime *itime;
IStatus *istatus;
ISockets *isocks;
INetwork *inet;
IPacketizer *ipacketizer;

bool has_robot = false;
double last_status_time = 0;
int hitpoints = 21;
unsigned short battery = 0;

double last_vf_request;
static GuiState gs;
bool showing_score = false;
bool inited_score = false;

#define POSE_UP_BUTTON 0    //  dpad up
#define POSE_DOWN_BUTTON 1  //  dpad down
#define TROT_UP_BUTTON 3    //  dpad right
#define TROT_DOWN_BUTTON 2  //  dpad left
#define TURN_LEFT_BUTTON 8  //  left shoulder
#define TURN_RIGHT_BUTTON 9 //  right shoulder
#define SHOW_MWSCORE_BUTTON 7


#define SPEED_AXIS 1    //  left Y
#define STRAFE_AXIS 0   //  left X
#define AIM_X_AXIS 2    //  right X
#define AIM_Y_AXIS 3    //  right Y
#define FIRE_RIGHT_AXIS 4    //  left trigger
#define FIRE_LEFT_AXIS 5    //  left trigger

#define MAXJOY 28000.0f
#define MINJOY 1000.0f  //  dead zone

float joyspeed = 0;
float joyturn = 0;
float joystrafe = 0;
float joyelevate = 0;
float joyheading = 0;
int joypose = 3;    //  0 .. 6
int joytrotix = 17;
int joyfire = 0; // 0x1 | 0x2

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
            if (js.value > -MINJOY && js.value < MINJOY) {
                js.value = 0;
            }
            if (js.number == SPEED_AXIS) {
                joyspeed = cap(js.value / - MAXJOY);
            }
            else if (js.number == STRAFE_AXIS) {
                joystrafe = cap(js.value / MAXJOY);
            }
            else if (js.number == AIM_X_AXIS) {
                joyheading = cap(js.value / MAXJOY);
            }
            else if (js.number == AIM_Y_AXIS) {
                joyelevate = cap(js.value / MAXJOY);
            }
            else if (js.number == FIRE_LEFT_AXIS) {
                joyfire = (joyfire & ~1) | (js.value > 10000 ? 1 : 0);
            }
            else if (js.number == FIRE_RIGHT_AXIS) {
                joyfire = (joyfire & ~2) | (js.value > 10000 ? 2 : 0);
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
            else if (js.number == TURN_RIGHT_BUTTON) {
                joyturn = on ? 1 : 0;
            }
            else if (js.number == TURN_LEFT_BUTTON) {
                joyturn = on ? -1 : 0;
            }
            else if (js.number == POSE_UP_BUTTON) {
                if (joypose < 6 && on) {
                    ++joypose;
                    std::cerr << "pose: " << joypose << std::endl;
                }
            }
            else if (js.number == POSE_DOWN_BUTTON) {
                if (joypose > 0 && on) {
                    --joypose;
                    std::cerr << "pose: " << joypose << std::endl;
                }
            }
            else if (js.number == SHOW_MWSCORE_BUTTON) {
                if (inited_score) {
                    showing_score = !showing_score;
                }
                else {
                    istatus->error("mwscore not configured");
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
    hitpoints = status->hits;
    battery = status->battery;
    /*
    istatus->message(std::string("Status: hits=") + boost::lexical_cast<std::string>((int)status->hits)
        + " status=" + hexnum(status->status) + " message=" + status->message);
     */
}

boost::shared_ptr<Image> last_image;
unsigned short last_image_seq;
double last_image_time = 0;

void do_videoframe(P_VideoFrame const *videoframe, size_t size) {
    /*
    istatus->message(std::string("Video frame: serial=") +
        boost::lexical_cast<std::string>((int)videoframe->serial) +
        ", width=" + boost::lexical_cast<std::string>((int)videoframe->width) +
        ", height=" + boost::lexical_cast<std::string>((int)videoframe->height) +
        ", size=" + boost::lexical_cast<std::string>(size));
     */
    last_image = boost::shared_ptr<Image>(new Image());
    void *d = last_image->alloc_compressed(size - sizeof(P_VideoFrame), true);
    memcpy(d, &videoframe[1], size - sizeof(P_VideoFrame));
    last_image->complete_compressed(size - sizeof(P_VideoFrame));
    last_image_time = itime->now();
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
            do_videoframe((P_VideoFrame const *)data, size);
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

void connect_mwscore(std::string const &addr) {
    size_t pos = addr.find_last_of(':');
    if (pos == 0 || pos == std::string::npos) {
        istatus->error("Bad mwscore address: " + addr);
        return;
    }
    std::string a(addr.substr(0, pos));
    int port(atoi(addr.substr(pos+1).c_str()));
    if (port < 1 || port > 65535) {
        istatus->error("Bad mwscore port: " + addr);
        return;
    }
    connect_score(a.c_str(), (unsigned short)port, istatus, isocks);
    inited_score = true;
}

int main(int argc, char const *argv[]) {

    itime = newclock();
    istatus = mkstatus(itime, true);
    isocks = mksocks(port, istatus);
    inet = scan(isocks, itime, istatus);
    ipacketizer = packetize(inet, istatus);

    boost::shared_ptr<Settings> theSettings(Settings::load("control.json"));
    if (theSettings->has_name("mwscore")) {
        connect_mwscore(theSettings->get_value("mwscore")->get_string());
    }

    joyopen();

    open_gui(gs, itime);

    double then = itime->now();
    double bc = 0;
    double lastQCheck = then;
    double q = 1;
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
            seti.strafe = joystrafe;
            seti.aimElevation = joyelevate;
            seti.aimHeading = joyheading;
            seti.pose = joypose;
            seti.fire = joyfire;
            ipacketizer->respond(C2R_SetInput, sizeof(seti), &seti);
            if (now > last_vf_request + VIDEO_REQUEST_INTERVAL) {
                P_RequestVideo rv;
                rv.width = VIDEO_REQUEST_WIDTH;
                rv.height = VIDEO_REQUEST_HEIGHT;
                rv.millis = (unsigned short)(VIDEO_REQUEST_INTERVAL * 1000 * 3);
                ipacketizer->respond(C2R_RequestVideo, sizeof(rv), &rv);
                last_vf_request = now;
            }
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
        if (now >= lastQCheck + Q_CHECK_INTERVAL) {
            lastQCheck = now;
            int lost = 0;
            int received = 0;
            inet->check_clear_loss(lost, received);
            if (lost == 0) {
                q = q * 0.9 + 0.1;
            }
            else {
                q = q * 0.9 + 0.1 * ((double)received / ((double)received + lost));
            }
        }
        gs.image = last_image;
        gs.image_old = (now - last_image_time > 0.2);
        gs.trot = trotvals[joytrotix];
        gs.pose = joypose;
        gs.hitpoints = hitpoints;
        gs.battery = battery;
        gs.loss = q > 1 ? 0 : q < 0 ? 255 : (255 - (unsigned char)(255 * q));
        step_score();
        if (showing_score) {
            show_gui_score(cur_score());
        }
        else {
            hide_gui_score();
        }
        update_gui(gs);
        step_gui();
    }

    return 1;
}
