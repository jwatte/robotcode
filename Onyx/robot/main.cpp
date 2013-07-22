
#include "itime.h"
#include "inetwork.h"
#include "istatus.h"
#include "protocol.h"
#include "util.h"
#include "Interpolator.h"
#include "logger.h"
#include "logstatus.h"

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

#define CMD_FIRE 0x12


const float CENTER_XPOS = 180.0f;
const float CENTER_YPOS = 120.0f;
const float CENTER_ZPOS = -30.0f;

#define MAX_SERVO_COUNT 16

bool REAL_USB = true;

static double const LOCK_ADDRESS_TIME = 5.0;
static double const STEP_DURATION = 0.008;

static unsigned short port = 6969;
static char my_name[32] = "Onyx";
static char pilot_name[32] = "";
unsigned short MAX_TORQUE = 900;

static ITime *itime;
static IStatus *istatus;
static ISockets *isocks;
static INetwork *inet;
static IPacketizer *ipackets;
static unsigned short battery;

static unsigned char firing_value;

legparams lparam;

struct initinfo {
    unsigned short id;
    unsigned short center;
};
static const initinfo init[] = {
    { 1, 2048+512 },
    { 2, 2048-256 },
    { 3, 2048-256 },
    { 4, 2048-512 },
    { 5, 2048+256 },
    { 6, 2048+256 },
    { 7, 2048-512 },
    { 8, 2048+256 },
    { 9, 2048+256 },
    { 10, 2048+512 },
    { 11, 2048-256 },
    { 12, 2048-256 },
    { 13, 2048 },
    { 14, 2048 },
};

static unsigned char nst = 0;

//  slew rates are amount of change per second
#define SPEED_SLEW 2.5f
#define HEIGHT_SLEW 25.0f

const float lift = 40;

float ctl_trot = 1.5f;
float ctl_speed = 0;
float ctl_turn = 0;
float ctl_strafe = 0;
float ctl_heading = 0;
float ctl_elevation = 0;
unsigned char ctl_pose = 3;
unsigned char ctl_fire = 0;

legpose last_pose[4];

//  STRAFE_SIZE is stroke each direction -- so stride is 2*STRAFE_SIZE
const float STRAFE_SIZE = 40.0f;
//  STEP_SIZE is stroke each direction -- so stride is 2*STEP_SIZE
const float STEP_SIZE = 80.0f;

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
            //lift less if almost standing still
            dz = dz * 10 * fabsf(speed);
        }
    }
    float xpos = CENTER_XPOS; // lparam.center_x + lparam.first_length + lparam.second_length;
    float ypos = CENTER_YPOS; // lparam.center_y + lparam.first_length;
    float zpos = CENTER_ZPOS - deltaPose;
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
        sst << "Could not solve leg: " << leg << " step " << step
            << " speed " << speed << " xpos " << xpos << " ypos " << ypos
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

void do_fire(ServoSet &ss) {
    unsigned char buf[3] = { CMD_FIRE, 0, 0 };
    if (firing_value & 1) {
        buf[1] = 50;
    }
    if (firing_value & 2) {
        buf[2] = 50;
    }
    ss.raw_cmd(buf, 3);
}


static void handle_discover(P_Discover const &pd) {
    P_Info ifo;
    memset(&ifo, 0, sizeof(ifo));
    safecpy(ifo.name, my_name);
    safecpy(ifo.pilot, pilot_name);
    ipackets->respond(R2C_Info, sizeof(ifo), &ifo);
}

static void handle_connect(P_Connect const &pc) {
    safecpy(pilot_name, pc.pilot);
    istatus->message("New pilot: " + std::string(pc.pilot));
    inet->lock_address(LOCK_ADDRESS_TIME);
}

static void handle_setinput(P_SetInput const &psi) {
    ctl_trot = psi.trot;
    ctl_speed = psi.speed;
    ctl_turn = psi.turn;
    ctl_strafe = psi.strafe;
    ctl_heading = psi.aimHeading;
    ctl_elevation = psi.aimElevation;
    ctl_pose = psi.pose;
    ctl_fire = psi.fire;
    /*
    fprintf(stderr, "\rtrot %4.2f  speed %4.2f  turn %4.2f  strafe %4.2f  head %4.2f  elev %4.2f  pose %2d  fire %0x2 ",
        ctl_trot, ctl_speed, ctl_turn, ctl_strafe, ctl_heading, ctl_elevation, ctl_pose, ctl_fire);
    fflush(stderr);
    */
}

double request_video_time;
int request_video_width;
int request_video_height;
unsigned short request_video_serial;

static void handle_requestvideo(P_RequestVideo const &prv) {
    request_video_time = itime->now() + prv.millis * 0.001;
    request_video_width = prv.width;
    request_video_height = prv.height;
}



static void handle_packets() {
    unsigned char type = 0;
    size_t size = 0;
    void const *packet = 0;
    bool want_status = false;
    while (ipackets->receive(type, size, packet)) {
        switch (type) {
        case C2R_Discover:
            if (size != sizeof(P_Discover)) {
                istatus->error("Bad message size P_Discover");
            }
            else {
                handle_discover(*(P_Discover const *)packet);
            }
            break;
        case C2R_Connect:
            if (size != sizeof(P_Connect)) {
                istatus->error("Bad message size P_Connect");
            }
            else {
                handle_connect(*(P_Connect const *)packet);
                want_status = true;
            }
            break;
        case C2R_SetInput:
            if (size != sizeof(P_SetInput)) {
                istatus->error("Bad message size P_SetInput");
            }
            else {
                handle_setinput(*(P_SetInput const *)packet);
                want_status = true;
            }
            break;
        case C2R_RequestVideo:
            if (size != sizeof(P_RequestVideo)) {
                istatus->error("Bad message size P_RequestVideo");
            }
            else {
                handle_requestvideo(*(P_RequestVideo const *)packet);
            }
            break;
        }
    }

    if (want_status) {
        struct P_Status ps;
        memset(&ps, 0, sizeof(ps));
        ps.battery = battery;
        ps.hits = 21;
        ps.status = nst;
        Message msg;
        bool got_message = false;
        //  If too many messages, drain some, but keep the latest error 
        //  I've seen, if any.
        while (istatus->n_messages() > 10) {
            Message msg2;
            istatus->get_message(msg2);
            if (msg2.isError) {
                msg = msg2;
                got_message = true;
            }
        }
        if (!got_message) {
            got_message = istatus->get_message(msg);
        }
        if (got_message) {
            safecpy(ps.message, msg.message.c_str());
        }
        ipackets->respond(R2C_Status, sizeof(ps), &ps);
    }
}

class ImageListener : public Listener {
public:
    ImageListener(boost::shared_ptr<Property> const &prop) :
        prop_(prop),
        dirty_(false)
    {
    }
    void on_change() {
        image_ = prop_->get<boost::shared_ptr<Image>>();
        dirty_ = true;
    }
    bool check_and_clear() {
        bool ret = dirty_;
        dirty_ = false;
        return ret;
    }
    boost::shared_ptr<Property> prop_;
    boost::shared_ptr<Image> image_;
    bool dirty_;
};


class USBLogger : public Logger {
public:
    virtual void log_data(LogWhat what, void const *data, size_t size)
    {
        log(what == LogUSBWrite ? LogKeyUSBOut : LogKeyUSBIn, data, size);
    }
};

void usb_thread_fn() {
    sched_param parm = { .sched_priority = 25 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "USBLink::thread_fn(): pthread_setschedparam(): " << err << std::endl;
    }
    get_leg_params(lparam);
    boost::shared_ptr<Logger> logger(new USBLogger());
    ServoSet ss(REAL_USB, logger);
    ss.set_power(15);
    for (size_t i = 0; i < sizeof(init)/sizeof(init[0]); ++i) {
        ss.add_servo(init[i].id, init[i].center);
    }
    ss.set_torque(MAX_TORQUE, 1); //  not quite top torque

    double thetime = 0, prevtime = 0, intime = read_clock();
    float step = 0;
    SlewRateInterpolator<float> i_speed(0, SPEED_SLEW, 1, intime);
    SlewRateInterpolator<float> i_strafe(0, SPEED_SLEW, 1, intime);
    SlewRateInterpolator<float> i_turn(0, SPEED_SLEW, 1, intime);
    SlewRateInterpolator<float> i_height(0, HEIGHT_SLEW, 1, intime);
    double frames = 0;
    while (true) {
        float use_trot = ctl_trot;
        float use_speed = ctl_speed;
        float use_turn = cap(ctl_turn + ctl_heading);
        float use_strafe = ctl_strafe;
        if (REAL_USB) {
            if (ss.torque_pending()) {
                use_speed = 0;
                use_trot = 0;
                use_turn = 0;
                use_strafe = 0;
            }
        }

        thetime = read_clock();
        frames = frames + 1;
        if (thetime - intime > 10) {
            fprintf(stderr, "usb fps: %.1f\n", frames / (thetime - intime));
            frames = 0;
            intime = thetime;
        }
        float dt = thetime - prevtime;

        i_speed.setTarget(use_speed);
        i_strafe.setTarget(use_strafe);
        i_turn.setTarget(use_turn);
        i_height.setTarget(ctl_pose * 10.0);

        if (dt >= STEP_DURATION) {
            i_speed.setTime(thetime);
            i_strafe.setTime(thetime);
            i_turn.setTime(thetime);
            i_height.setTime(thetime);

            //  don't fall more than 0.1 seconds behind, else catch up in one swell foop
            if (dt < 0.1f) {
                step += dt * 100 * use_trot;
                prevtime += dt;
            }
            else {
                //  and don't update step!
                prevtime = thetime;
                dt = 0;
            }
            //  "step" is a measure of the cycle in percent
            while (step >= 100) {
                step -= 100;
            }
            while (step < 0) {
                step += 100;
            }
            if (i_speed.get() == 0 && i_turn.get() == 0 && i_strafe.get() == 0) {
                //  when standing still, start at a known pos
                step = 0;
            }
            poselegs(ss, step, i_speed.get(), -i_turn.get(), i_strafe.get(), i_height.get());
            if (ctl_fire || (firing_value != ctl_fire)) {
                firing_value = ctl_fire;
                do_fire(ss);
            }
        }
        else {
            usleep(3000);
        }
        ss.step();
        battery = ss.battery();
        log(LogKeyBattery, battery);
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
        log(LogKeyTemperature, status, MAX_SERVO_COUNT);
        for (size_t i = 0; i != MAX_SERVO_COUNT; ++i) {
            if (status[i] > 75) {
                std::stringstream strstr;
                strstr << "Temp for servo " << (int)i << " is " << (int)status[i];
                istatus->error(strstr.str());
                system("bld/obj/off");
                flush_logger();
                exit(1);
            }
        }

        static int preg = 0;
        ++preg;
        if (!(preg & 63)) {
            std::cerr << "\rTemp:";
            for (size_t i = 1; i != 14; ++i) {
                std::cerr << " " << (int)status[i];
            }
        }

        unsigned short status2[MAX_SERVO_COUNT] = { 0 };
        ss.slice_reg2(REG_CURRENT, status2, MAX_SERVO_COUNT);
        log(LogKeyCurrent, status2, MAX_SERVO_COUNT * sizeof(unsigned short));
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
        else {
usage:
            fprintf(stderr, "usage: robot [--fakeusb] [--maxtorque 1023]\n");
            exit(1);
        }
    }
    itime = newclock();
    IStatus *chain = mkstatus(itime, true);
    istatus = mk_logstatus(chain);
    isocks = mksocks(port, istatus);
    inet = listen(isocks, itime, istatus);
    ipackets = packetize(inet, istatus);

    open_logger();
    log_ratelimit(LogKeyError, false);

    boost::shared_ptr<boost::thread> usb_thread(new boost::thread(boost::bind(usb_thread_fn)));

    boost::shared_ptr<Settings> settings(Settings::load("onyx.json"));
    boost::shared_ptr<Module> camera(Camera::open(settings->get_value("camera")));
    boost::shared_ptr<Property> image(camera->get_property_named("image"));
    boost::shared_ptr<ImageListener> image_listener(new ImageListener(image));
    image->add_listener(image_listener);

    double thetime = 0, intime = read_clock();
    double frames = 0;
    while (true) {

        camera->step();
        ipackets->step();
        if (inet->check_clear_overflow()) {
            //  don't send bulky video if I'm out of send space
            request_video_time = 0;
        }
        handle_packets();

        thetime = read_clock();
        frames = frames + 1;
        if (thetime - intime > (REAL_USB ? 20 : 2)) {
            fprintf(stderr, "main fps: %.1f  battery: %.1f\n", frames / (thetime - intime),
                (float)battery / 100.0);
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

        if (image_listener->check_and_clear()) {
            iovec iov[2];
            ++request_video_serial;
            Image &img = *image_listener->image_;
            if (thetime < request_video_time) {
                P_VideoFrame vf;
                memset(&vf, 0, sizeof(vf));
                vf.serial = request_video_serial;
                vf.width = img.width();
                vf.height = img.height();
                memset(iov, 0, sizeof(iov));
                iov[0].iov_base = &vf;
                iov[0].iov_len = sizeof(vf);
                iov[1].iov_base = const_cast<void *>(img.bits(CompressedBits));
                iov[1].iov_len = img.size(CompressedBits);
                ipackets->vrespond(R2C_VideoFrame, 2, iov);
            }
            /*
            if (!*(unsigned char *)img.bits(FullBits)) {
                //  no-op, just add decompress overhead
                iov[0].iov_len = 0;
            }
            */
        }
        usleep(1000);
    }
    return 0;
}

