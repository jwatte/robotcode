
#include "itime.h"
#include "inetwork.h"
#include "istatus.h"
#include "protocol.h"
#include "util.h"
#include <assert.h>


static double const LOCK_ADDRESS_TIME = 5.0;

static unsigned short port = 6969;
static char my_name[32] = "Onyx";
static char pilot_name[32] = "";

static ITime *itime;
static IStatus *istatus;
static ISockets *isocks;
static INetwork *inet;
static IPacketizer *ipackets;


static void handle_discover(P_Discover const &pd) {
    P_Info ifo;
    memset(&ifo, 0, sizeof(ifo));
    safecpy(ifo.name, my_name);
    safecpy(ifo.pilot, pilot_name);
    ipackets->send(R2C_Info, sizeof(ifo), &ifo);
}

static void handle_connect(P_Connect const &pc) {
    safecpy(pilot_name, pc.pilot);
    istatus->message("New pilot: " + std::string(pc.pilot));
    inet->lock_address(LOCK_ADDRESS_TIME);
}

static void handle_setinput(P_SetInput const &psi) {
    //  TODO: implement set input
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
        ps.status = 0;
        std::string msg;
        bool got_message = false;
        //  If too many messages, drain some, but keep the latest error 
        //  I've seen, if any.
        while (istatus->n_messages() > 10) {
            std::string msg2;
            bool is_error = false;
            istatus->get_message(is_error, msg2);
            if (is_error) {
                msg = msg2;
                got_message = true;
            }
        }
        if (!got_message) {
            bool is_error = false;
            got_message = istatus->get_message(is_error, msg);
        }
        if (got_message) {
            safecpy(ps.message, msg.c_str());
        }
        ipackets->send(R2C_Status, sizeof(ps), &ps);
    }
}

int main(int argc, char const *argv[]) {
    itime = newclock();
    istatus = mkstatus();
    isocks = mksocks(port, istatus);
    inet = listen(isocks, itime, istatus);
    ipackets = packetize(inet, istatus);
    double then = itime->now();
    while (true) {
        ipackets->step();
        handle_packets();
        double now = itime->now();
        assert(now > then);
        if (now - then < 0.15) {
            itime->sleep(0.15 - (now - then));
        }
        then = now;
    }
    return 0;
}

