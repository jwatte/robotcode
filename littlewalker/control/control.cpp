
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/signal.h>

#include <libusb.h>
#include <openssl/md5.h>

#include <unordered_map>
#include <list>


#define CONTROL_PORT 7331


#define RIGHT_CENTER 3200
#define LEFT_CENTER 3200
#define CENTER_CENTER 3100

//  in half-microseconds
#define PWM_FREQ 30000

#define WALK_EXTENT 560
#define LIFT_EXTENT 800


#define DATA_INFO_EPNUM 0x81
#define DATA_IN_EPNUM 0x82
#define DATA_OUT_EPNUM 0x03

#define CMD_DDR 1
#define CMD_POUT 2
#define CMD_PIN 3
#define CMD_TWOBYTEARG 4
#define CMD_PWMRATE CMD_TWOBYTEARG
#define CMD_SETPWM 5
#define CMD_WAIT 6
#define CMD_LERPPWM 7



struct packet_hdr {
    unsigned char cmd;
};
char const *myname;
bool verbose = false;

float g_forward;
float g_turn;
int g_phase = -1;


volatile bool interrupted;

void onintr(int) {
    interrupted = true;
}


enum ctr_id {
    ctrFastErrors = 0,
    ctrSlowErrors = 1,
    ctrPackets = 2,
    ctrPings = 3,
    ctrForward = 4,
    ctrTurn = 5
};

struct counter {
    counter(ctr_id cid, char const *str, double decay = 0.9) : id_(cid), name_(str), value_(0), decay_(0.9) {
        next_ = first_;
        first_ = this;
        ++count_;
    }

    static void decay() {
        for (counter *f = first_; f != NULL; f = f->next_) {
            f->step();
        }
    }
    static counter *first_;
    static int count_;
    counter *next_;
    ctr_id id_;
    char const *name_;
    double value_;
    double decay_;

    void update(double value) {
        value_ = value_ + value;
    }
    void set(double value) {
        value_ = value;
    }
    void step() {
        value_ = value_ * decay_;
    }
};

counter *counter::first_;
int counter::count_;

counter ctr_fast_errors(ctrFastErrors, "errors.fast", 0);
counter ctr_slow_errors(ctrSlowErrors, "errors.slow", 0.99);
counter ctr_packets(ctrPackets, "packets.count");
counter ctr_pings(ctrPings, "packets.ping.count");
counter ctr_forward(ctrForward, "movement.forward", 1);
counter ctr_turn(ctrTurn, "movement.turn", 1);


double now() {
    struct timespec tspec;
    clock_gettime(CLOCK_MONOTONIC, &tspec);
    return (double)tspec.tv_sec + (double)tspec.tv_nsec * 1.0e-9;
}

void getaddr(sockaddr_in const *from, char *obuf) {
    unsigned char const *ptr = (unsigned char const *)&from->sin_addr;
    unsigned short port = htons(from->sin_port);
    sprintf(obuf, "%d.%d.%d.%d:%d", ptr[0], ptr[1], ptr[2], ptr[3], port);
}


void geterrcnt(libusb_device_handle *dh) {
    int x = 0;
    unsigned char err = 0;
    int i = libusb_bulk_transfer(dh, DATA_INFO_EPNUM, &err, 1, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error getting DATA_INFO_EPNUM: %d (%s)\n", i, libusb_error_name(i));
        exit(14);
    }
    if (err > 0) {
        fprintf(stderr, "There have been %d errors since last check.\n", err);
    }
    else {
        if (verbose) {
            fprintf(stderr, "No errors so far.\n");
        }
    }
}

libusb_context *ctx = 0;
libusb_device_handle *dh = 0;

void init_usb() {
    int i = libusb_init(&ctx);
    if (i != 0) {
        fprintf(stderr, "libusb_init() failed\n");
        exit(1);
    }
    libusb_set_debug(ctx, 3);

    dh = libusb_open_device_with_vid_pid(ctx, 0xf000, 0x0002);
    if (!dh) {
        fprintf(stderr, "Could not find device 0xf000 / 0x0002\n");
        exit(2);
    }

    i = libusb_claim_interface(dh, 0);
    if (i != 0) {
        fprintf(stderr, "Could not claim interface 0: %d\n", i);
        exit(3);
    }

    unsigned char cmd[64];
    int x = 0;
    cmd[0] = (CMD_DDR << 4) | 0;
    cmd[1] = 0xff;
    cmd[2] = (CMD_POUT << 4) | 0;
    cmd[3] = 0;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 20, &x, 1000);
    if (i != 0) {
        fprintf(stderr, "Could not configure pin directions: %d\n", i);
        exit(4);
    }
    geterrcnt(dh);

    //  turn on pwm
    cmd[0] = (4 << 4);  //  CMD_PWMRATE
    cmd[1] = (PWM_FREQ >> 8) & 0xff;
    cmd[2] = PWM_FREQ & 0xff;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 3, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error turning on PWM: %d (%s)\n", i, libusb_error_name(i));
        exit(5);
    }
    geterrcnt(dh);
}

enum ledtype {
    lRunning = 0,
    lConnected = 1,
    lForward = 2,
    lBackward = 3
};
unsigned char oldledState;
unsigned char ledState;
void setled(ledtype led, bool on) {
    if (on) {
        ledState |= (1 << led);
    }
    else {
        ledState &= ~(1 << led);
    }
}

void updateled() {
    if (oldledState != ledState) {
        unsigned char cmd[10];
        cmd[0] = (CMD_POUT << 4) | 2;   //  D port
        cmd[1] = ledState;
        int x, i;
        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 2, &x, 1000);
        if (i < 0) {
            fprintf(stderr, "error setting LED: %d (%s)\n", i, libusb_error_name(i));
            exit(100);
        }
        oldledState = ledState;
        if (verbose) {
            fprintf(stderr, "LED state 0x%02x\n", ledState);
        }
    }
}

void rest() {
    g_forward = 0;
    g_turn = 0;
    unsigned char cmd[64];
    //  set to rest values
    int n = 0;
    for (int i = 0; i < 8; ++i) {
        cmd[n++] = (5 << 4) | i;
        unsigned short val = 3500 - (i & 1) * 400;
        cmd[n++] = (val >> 8) & 0xff;
        cmd[n++] = val & 0xff;
    }
    int x;
    int i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, n, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error writing PWM data: %d (%s)\n", i, libusb_error_name(i));
        exit(8);
    }
    geterrcnt(dh);
}

void walk() {

    static struct {
        float left;
        float center;
        float right;
        int delay;
    }
    gait[] = {
        { -WALK_EXTENT, -LIFT_EXTENT, -WALK_EXTENT, 200000 },
        { WALK_EXTENT, -LIFT_EXTENT, WALK_EXTENT, 300000 },
        { WALK_EXTENT, LIFT_EXTENT, WALK_EXTENT, 200000 },
        { -WALK_EXTENT, LIFT_EXTENT, -WALK_EXTENT, 300000 },
    };
    unsigned char cmd[64];
    size_t phase = -1;
    long delay = 0;
    int i, x;
    while (!interrupted) {
        phase = phase + 1;
        if (phase >= sizeof(gait)/sizeof(gait[0])) {
            phase = 0;
        }
        delay = gait[phase].delay * 4;
        printf("phase %ld delay %ld\n", (long)phase, (long)delay);
        printf("left %f\n", gait[phase].left);
        unsigned short t = (unsigned short)(RIGHT_CENTER + gait[phase].right);
        int n = 0;
        cmd[n++] = (CMD_LERPPWM << 4) | 0;
        cmd[n++] = (t >> 8) & 0xff;
        cmd[n++] = t & 0xff;
        cmd[n++] = delay / PWM_FREQ;
        t = (unsigned short)(CENTER_CENTER + gait[phase].center);
        cmd[n++] = (CMD_LERPPWM << 4) | 1;
        cmd[n++] = (t >> 8) & 0xff;
        cmd[n++] = t & 0xff;
        cmd[n++] = delay / PWM_FREQ;
        t = (unsigned short)(LEFT_CENTER + gait[phase].left);
        cmd[n++] = (CMD_LERPPWM << 4) | 2;
        cmd[n++] = (t >> 8) & 0xff;
        cmd[n++] = t & 0xff;
        cmd[n++] = delay / PWM_FREQ;
        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, n, &x, 1000);
        if (i < 0) {
            fprintf(stderr, "error stepping PWM: %d (%s)\n", i, libusb_error_name(i));
            exit(9);
        }
        geterrcnt(dh);
        usleep(delay / 2);
    }
}


int sock;

void init_socket() {
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket()");
        exit(10);
    }
    int so = 1;
    socklen_t olen = sizeof(so);
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &so, olen) < 0) {
        perror("SO_REUSEADDR");
        exit(11);
    }
    if (setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &so, olen) < 0) {
        perror("SO_BROADCAST");
        exit(11);
    }
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(CONTROL_PORT);
    if (bind(sock, (sockaddr const *)&sin, sizeof(sin)) < 0) {
        perror("bind()");
        exit(11);
    }
}

//  Make really sure that this packet is intended for my 
//  particular protocol.
bool verify_csum(void const *packet, size_t size) {
    if (size < 5) {
        return false;
    }
    unsigned char save[4];
    memcpy(save, packet, 4);
    ((char *)packet)[0] = 'r';
    ((char *)packet)[1] = 'o';
    ((char *)packet)[2] = 'b';
    ((char *)packet)[3] = 'o';
    unsigned char dig[16];
    memset(dig, 0, sizeof(dig));
    MD5((unsigned char const *)packet, size, dig);
    if (memcmp(save, dig, 4)) {
        //  checksum failed
        return false;
    }
    return true;
}

char spacket[65536];

void do_send(void const *buf, size_t size, sockaddr_in const *to) {
    if (size > sizeof(spacket) - 4) {
        fprintf(stderr, "too long packet (%d bytes) in do_send()\n", (int)size);
        exit(100);
    }
    memcpy(&spacket[4], buf, size);
    spacket[0] = 'r';
    spacket[1] = 'o';
    spacket[2] = 'b';
    spacket[3] = 'o';
    unsigned char sig[16];
    memset(sig, 0, sizeof(sig));
    MD5((unsigned char const *)spacket, size + 4, sig);
    memcpy(spacket, sig, 4);
    int s = ::sendto(sock, spacket, size + 4, 0, (sockaddr const *)to, sizeof(sockaddr_in));
    if (s < 0) {
        fprintf(stderr, "sendto() of %d bytes failed.\n", (int)size + 4);
        exit(100);
    }
}


enum {
    cPing = 1,
    cPong = 2,
    cControl = 3,
    cFire = 5,
    cReports = 6,
    cCamera = 7,
    cPower = 8,
    cReport = 9,
    cFrame = 10
};

enum {
    typeController = 1,
    typeRobot = 2
};

struct cmd_ping : public packet_hdr {
    unsigned char ping_type;
    unsigned char slen;
};

struct cmd_pong : public packet_hdr {
    unsigned char pong_type;
    unsigned char slen;
};

struct cmd_control : public packet_hdr {
    //  negative means backwards
    //  4096 is "nominal"
    short speed;
    short rate;
};

struct cmd_fire : public packet_hdr {
    //  number of shots to fire
    unsigned char num_shots;
};

struct cmd_reports : public packet_hdr {
    //  milliseconds, 0 for off
    unsigned short interval;
    //  after num_reports, will shut off reporting
    unsigned char num_reports;
};

struct cmd_camera : public packet_hdr {
    //  0 for off; after num_frames, will shut off streaming
    unsigned char num_frames;
};

struct cmd_power : public packet_hdr {
    //  0 for off, 255 for on
    unsigned char power;
};


struct report_info {
    sockaddr_in sin;
    unsigned short interval;
    unsigned char num_left;
    double last_report;
};

struct sockaddr_in_h {
    sockaddr_in sin;
    inline bool operator==(sockaddr_in_h const &o) const {
        return !memcmp(&sin.sin_addr, &o.sin.sin_addr, 4) && sin.sin_port == o.sin.sin_port;
    }
};
namespace std {
template<>
struct hash<sockaddr_in_h> {
    inline unsigned int operator()(sockaddr_in_h const &sa) const {
        return *(unsigned int const *)&sa.sin.sin_addr ^ (sa.sin.sin_port * 2011);
    }
};
}

std::unordered_map<sockaddr_in_h, report_info> reports;

enum {
    ptString = 1,
    ptByte = 2,
    ptShort = 3,
    ptFloat = 4,
};

struct cmd_report_param {
    unsigned char type; //  data type of param
    unsigned char code; //  "name" of param
    unsigned char data[1];  //  actual data, based on type
};

struct cmd_report : public packet_hdr {
    unsigned char num_params;
    cmd_report_param params[1];
};

struct cmd_frame : public packet_hdr {
    //  correlate milliseconds
    unsigned short millis;
    //  MJPEG data
    unsigned char data[1];   //  actually, often very big
};


void handle_ping(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    ctr_pings.update(1);
    char buf[256];
    memcpy(buf, &((cmd_ping *)hdr)[1], ((cmd_ping *)hdr)->slen);
    buf[((cmd_ping *)hdr)->slen] = 0;
    if (verbose) {
        fprintf(stderr, "got ping from %s\n", buf);
    }
    cmd_pong *cp = (cmd_pong *)buf;
    cp->cmd = cPong;
    cp->pong_type = typeRobot;
    cp->slen = strlen(myname);
    memcpy(&cp[1], myname, cp->slen);
    do_send(buf, sizeof(*cp) + cp->slen, from);
}

void handle_control(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    g_forward = ((cmd_control const *)hdr)->speed / 4096.0;
    g_turn = ((cmd_control const *)hdr)->rate / 4096.0;
}

void handle_fire(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
}

void handle_reports(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    cmd_reports const *crep = (cmd_reports const *)hdr;
    sockaddr_in_h sinh;
    sinh.sin = *from;
    report_info ri;
    ri.sin = *from;
    ri.interval = crep->interval;
    ri.num_left = crep->num_reports;
    ri.last_report = 0;
    reports[sinh] = ri;
}

void handle_camera(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
}

void handle_power(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
}

struct cmd_handler {
    unsigned char cmd;
    size_t min_size;
    void (*handle)(packet_hdr const *hdr, size_t size, sockaddr_in const *from);
};

cmd_handler handlers[] = {
    { cPing, sizeof(cmd_ping), &handle_ping },
    { cControl, sizeof(cmd_control), &handle_control },
    { cFire, sizeof(cmd_fire), &handle_fire },
    { cReports, sizeof(cmd_reports), &handle_reports },
    { cCamera, sizeof(cmd_camera), &handle_camera },
    { cPower, sizeof(cmd_power), &handle_power },
};

void dispatch_packet(void const *packet, size_t size, sockaddr_in const *from) {
    ctr_packets.update(1);
    char ipaddr[30];
    getaddr(from, ipaddr);
    if (verbose) {
        fprintf(stderr, "%d bytes from %s\n", (int)size, ipaddr);
    }
    if (!verify_csum(packet, size)) {
        //  not intended for my kind
        fprintf(stderr, "packet checksum was wrong from %s\n", ipaddr);
        return;
    }
    packet_hdr const *hdr = (packet_hdr const *)((char const *)packet + 4);
    for (size_t i = 0, n = sizeof(handlers)/sizeof(handlers[0]); i != n; ++i) {
        if (handlers[i].cmd == hdr->cmd) {
            if (size >= handlers[i].min_size) {
                if (verbose) {
                    fprintf(stderr, "handling packet %d from %s\n", hdr->cmd, ipaddr);
                }
                (*handlers[i].handle)(hdr, size, from);
                return;
            }
            else {
                //  got packet from different version
                fprintf(stderr, "packet was too short from %s\n", ipaddr);
                break;
            }
        }
    }
    if (verbose) {
        fprintf(stderr, "packet type %d from %s not recognized\n", hdr->cmd, ipaddr);
    }
}

char dpacket[65536];
int dpacketlen;
double last_packet;

void recv_one() {
    last_packet = now();
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    socklen_t slen = sizeof(sin);
    dpacketlen = ::recvfrom(sock, dpacket, sizeof(dpacket), 0, (sockaddr *)&sin, &slen);
    if (dpacketlen > 0) {
        dispatch_packet(dpacket, dpacketlen, &sin);
    }
}

void poll_socket() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(sock, &fds);
    struct timeval tv = { 0, 10000 };
    if (select(sock+1, &fds, 0, 0, &tv) > 0) {
        recv_one();
    }
}

double walk_advance() {
    g_phase += 1;
    if (g_phase >= 4) {
        g_phase = 0;
    }
    float ltarget = -0.25;
    float ctarget = 0;
    float rtarget = -0.25;
    double delay = 0.01;
    switch (g_phase) {
        case 0:
            ltarget = -1;
            ctarget = -1;
            rtarget = -1;
            delay = 0.8;
            break;
        case 1:
            ltarget = 1;
            ctarget = -1;
            rtarget = 1;
            delay = 1.2;
            break;
        case 2:
            ltarget = 1;
            ctarget = 1;
            rtarget = 1;
            delay = 0.8;
            break;
        case 3:
            ltarget = -1;
            ctarget = 1;
            rtarget = -1;
            delay = 1.2;
            break;
        default:
            break;
    }
    if (g_forward > -0.1 && g_forward < 0.1) {
        //  am I turning?
        if (g_turn > 0.1 || g_turn < -0.1) {
            ltarget *= g_forward + g_turn * 2;
            rtarget *= g_forward - g_turn * 2;
        }
        else {
            //  stand still
            ltarget = -0.25;
            rtarget = -0.25;
            ctarget = 0;
        }
    }
    else {
        ltarget *= g_forward + g_turn * 2;
        rtarget *= g_forward - g_turn * 2;
    }
    if (ltarget > 1) {
        ltarget = 1;
    }
    if (ltarget < -1) {
        ltarget = -1;
    }
    if (rtarget > 1) {
        rtarget = 1;
    }
    if (rtarget < -1) {
        rtarget = -1;
    }
    unsigned char cmd[64];
    int n = 0;
    unsigned char time = delay * (2000000.0 / PWM_FREQ);
    unsigned short t = LEFT_CENTER + ltarget * WALK_EXTENT;
    cmd[n++] = (CMD_LERPPWM << 4) | 0;
    cmd[n++] = (t >> 8) & 0xff;
    cmd[n++] = t & 0xff;
    cmd[n++] = time;
    t = CENTER_CENTER + ctarget * LIFT_EXTENT;
    cmd[n++] = (CMD_LERPPWM << 4) | 1;
    cmd[n++] = (t >> 8) & 0xff;
    cmd[n++] = t & 0xff;
    cmd[n++] = time;
    t = RIGHT_CENTER + rtarget * WALK_EXTENT;
    cmd[n++] = (CMD_LERPPWM << 4) | 2;
    cmd[n++] = (t >> 8) & 0xff;
    cmd[n++] = t & 0xff;
    cmd[n++] = time;
    int x;
    int i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, n, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error stepping PWM: %d (%s)\n", i, libusb_error_name(i));
        exit(9);
    }
    return delay;
}

char report[65532];

void update_report(sockaddr_in const *sin) {
    cmd_report *cr = (cmd_report *)report;
    int n = 2;
    cr->cmd = cReport;
    cr->num_params = counter::count_;
    for (counter const *c = counter::first_; c != NULL; c = c->next_) {
        report[n++] = c->id_;
        report[n++] = ptFloat;
        float val(c->value_);
        memcpy(&report[n], &val, sizeof(val));
        n += 4;
    }
    do_send(report, n, sin);
}

void do_report(double n) {
    std::list<std::unordered_map<sockaddr_in_h, report_info>::iterator> toremove;
    for (auto ptr(reports.begin()), end(reports.end()); ptr != end; ++ptr) {
        if ((*ptr).second.last_report + (*ptr).second.interval * 0.001 < n) {
            update_report(&(*ptr).first.sin);
            (*ptr).second.last_report = n;
            (*ptr).second.num_left -= 1;
            if ((*ptr).second.num_left == 0) {
                toremove.push_back(ptr);
            }
        }
    }
    for (auto ptr(toremove.begin()), end(toremove.end()); ptr != end; ++ptr) {
        reports.erase(*ptr);
    }
}

double last_errcheck = 0;
double next_walk = 0;
double last_decay = 0;
double last_report = 0;

void robot_worker() {
    double n = now();
    if (n - last_errcheck > 0.5) {
        geterrcnt(dh);
        last_errcheck = n;
    }
    if (n >= next_walk) {
        next_walk = n + walk_advance();
    }
    if (n - last_decay > 1) {
        setled(lRunning, true);
        counter::decay();
        last_decay = n;
    }
    if (n - last_packet < 1.0) {
        setled(lConnected, true);
    }
    else {
        setled(lConnected, false);
    }
    if (n - last_report > 0.1) {
        do_report(n);
    }
    ctr_forward.set(g_forward);
    setled(lForward, g_forward > 0.1);
    setled(lBackward, g_forward < -0.1);
    ctr_turn.set(g_turn);
    updateled();
}

void init_led() {
    unsigned char cmd[200];
    int n = 0;
    cmd[n++] = (CMD_DDR << 4) | 2;
    cmd[n++] = 0xff;
    cmd[n++] = (CMD_POUT << 4) | 2;
    cmd[n++] = 0;
    int x;
    int i = libusb_bulk_transfer(dh, DATA_INFO_EPNUM, cmd, n, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error setting LED DDR: %d (%s)\n", i, libusb_error_name(i));
        exit(14);
    }
}

int main(int argc, char const *argv[]) {
    if (argc > 1) {
        myname = argv[1];
        ++argv;
        --argc;
    }
    else {
        myname = getenv("ROBONAME");
    }
    if (!myname) {
        fprintf(stderr, "specify name as first argument, or ROBONAME environment variable.\n");
        exit(1);
    }
    if (strlen(myname) > 64) {
        fprintf(stderr, "too long robot name: %d\n", (int)strlen(myname));
        exit(1);
    }
    if (strlen(myname) < 1) {
        fprintf(stderr, "robot name cannot be empty.\n");
        exit(1);
    }

    init_usb();
    signal(SIGINT, onintr);
    init_socket();
    init_led();

    rest();

    while (!interrupted) {
        poll_socket();
        robot_worker();
    }

    return 0;
}

