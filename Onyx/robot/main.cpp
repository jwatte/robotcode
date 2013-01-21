
#include "itime.h"
#include "inetwork.h"
#include "istatus.h"
#include "protocol.h"
#include "util.h"
#include <assert.h>

#include "ServoSet.h"
#include "IK.h"
#include "util.h"
#include <iostream>
#include <time.h>
#include <string.h>


static double const LOCK_ADDRESS_TIME = 5.0;

static unsigned short port = 6969;
static char my_name[32] = "Onyx";
static char pilot_name[32] = "";

static ITime *itime;
static IStatus *istatus;
static ISockets *isocks;
static INetwork *inet;
static IPacketizer *ipackets;

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

static unsigned char nst = 0;

#define SPEED_SLEW 2.0f

const float stride = 200;
const float lift = 40;
const float height_above_ground = 80;

float ctl_trot = 1.5f;
float ctl_speed = 0;
float ctl_turn = 0;
float ctl_strafe = 0;
float ctl_heading = 0;
float ctl_elevation = 0;
unsigned char ctl_pose = 1;

void poseleg(ServoSet &ss, int leg, float step, float speed, float strafe, float deltaPose) {
    float dx = 0, dy = 0, dz = 0;
    if (step < 50) {    //  front-to-back
        dx = (100 - 4 * step) * strafe;
        dy = (100 - 4 * step) * speed;
        dz = 0;
    }
    else {  //  lifted, back-to-front
        dx = (step * 4 - 300) * strafe;
        dy = (step * 4 - 300) * speed;
        dz = sinf((step - 50) * M_PI / 50) * lift;
        if (std::max(fabsf(speed), fabsf(strafe)) < 0.1) {
            dz = dz * 10 * fabsf(speed);
        }
    }
    float xpos = lparam.center_x + lparam.first_length + lparam.second_length;
    float ypos = lparam.center_y + lparam.first_length;
    float zpos = -height_above_ground - deltaPose;
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
    //  TODO: implement set input
    ctl_trot = psi.trot;
    ctl_speed = psi.speed;
    ctl_turn = psi.turn;
    ctl_strafe = psi.strafe;
    ctl_heading = psi.aimHeading;
    ctl_elevation = psi.aimElevation;
    ctl_pose = psi.pose;
}

static void handle_requestvideo(P_RequestVideo const &prv) {
    //  TODO: implement request video
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
        ps.hits = 0;
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

int main(int argc, char const *argv[]) {
    itime = newclock();
    istatus = mkstatus(itime, true);
    isocks = mksocks(port, istatus);
    inet = listen(isocks, itime, istatus);
    ipackets = packetize(inet, istatus);

    get_leg_params(lparam);
    ServoSet ss;
    for (size_t i = 0; i < sizeof(init)/sizeof(init[0]); ++i) {
        ss.add_servo(init[i].id, init[i].center);
    }
    ss.set_torque(900); //  not quite top torque

    double thetime = 0, prevtime = 0, intime = read_clock();
    float step = 0;
    float prevspeed = 0;
    float prevstrafe = 0;
    double frames = 0;
    while (true) {
        ipackets->step();
        handle_packets();
        thetime = read_clock();
        frames = frames + 1;
        if (thetime - intime > 20) {
            fprintf(stderr, "fps: %.1f\n", frames / (thetime - intime));
            frames = 0;
            intime = thetime;
        }
        float dt = thetime - prevtime;
        float use_trot = ctl_trot;
        float use_speed = ctl_speed;
        float use_turn = cap(ctl_turn + ctl_heading);
        float use_strafe = ctl_strafe;
        if (ss.torque_pending()) {
            use_speed = 0;
            use_trot = 0;
            use_turn = 0;
            use_strafe = 0;
        }
        if (dt >= 0.01) {
            //  don't fall more than 0.1 seconds behind, else catch up in one swell foop
            if (dt < 0.1f) {
                step += dt * 100 * use_trot;
                prevtime += 0.01;
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
            if (use_speed > prevspeed) {
                use_speed = std::min(prevspeed + dt * SPEED_SLEW, use_speed);
            }
            else if (use_speed < prevspeed) {
                use_speed = std::max(prevspeed - dt * SPEED_SLEW, use_speed);
            }
            if (use_strafe > prevstrafe) {
                use_strafe = std::min(prevstrafe + dt * SPEED_SLEW, use_strafe);
            }
            else if (use_strafe < prevstrafe) {
                use_strafe = std::max(prevstrafe - dt * SPEED_SLEW, use_strafe);
            }
            prevspeed = use_speed;
            prevstrafe = use_strafe;
            poselegs(ss, step, use_speed, -use_turn, use_strafe, ctl_pose * 30 - 30);
        }
        ss.step();
        if (ss.queue_depth() > 30) {
            istatus->error("Servo queue overflow -- flushing.");
            int n = 100;
            while (ss.queue_depth() > 0 && n > 0) {
                --n;
                ss.step();
            }
        }
        unsigned char status[33];
        unsigned char st = ss.get_status(status, 33);
        if (st != nst) {
            std::cerr << "status: " << hexnum(st) << std::endl;
            nst = st;
        }
        usleep(500);
    }
    return 0;
}

