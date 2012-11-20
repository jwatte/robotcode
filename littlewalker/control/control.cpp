
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
#include <time.h>

#include <openssl/md5.h>

#include <unordered_map>
#include <vector>
#include <list>

#include "../lib/defs.h"
#include "../lib/usb.h"
#include "video.h"

char const *myname;
bool verbose = false;

float g_forward;
float g_turn;
float g_panHeading;
float g_panTilt;
int g_phase = -1;
double delay_save = 0;

struct setting {
    setting(int t, char const *name) : t_(t), name_(name) {
        next_ = all_;
        all_ = this;
    }
    void set(int v) {
        t_ = v;
    }
    int t_;
    char const *name_;
    static setting *all_;
    setting *next_;

    operator int() const { return t_; }
};

setting tune_l_center(LEFT_CENTER, "tune_l_center");
setting tune_c_center(CENTER_CENTER, "tune_c_center");
setting tune_r_center(RIGHT_CENTER, "tune_r_center");
setting tune_walk_extent(WALK_EXTENT, "tune_walk_extent");
setting tune_lift_extent(LIFT_EXTENT, "tune_lift_extent");
setting tune_speed(1000, "tune_speed");

setting *setting::all_;

void settings_all(void (*func)(setting &set, void *arg), void *arg) {
    for (setting *s = setting::all_; s != NULL; s = s->next_) {
        func(*s, arg);
    }
}

void save_func(setting &set, void *file) {
    fprintf((FILE *)file, "%s=%d\n", set.name_, set.t_);
}

void setting_save_all(char const *name) {
    FILE *file = fopen(name, "wb");
    if (!file) {
        fprintf(stderr, "Could not save settings to %s\n", name);
        return;
    }
    fprintf(file, "\n");
    settings_all(save_func, file);
    fclose(file);
}

void load_func(setting &set, void *arg) {
    char buf[100];
    sprintf(buf, "\n%s=", set.name_);
    char *val = strstr((char *)arg, buf);
    if (val) {
        val += strlen(buf);
        set.t_ = atoi(val);
    }
}

void setting_load_all(char const *name) {
    FILE *file = fopen(name, "rb");
    if (!file) {
        fprintf(stderr, "Could not load settings from %s\n", name);
        return;
    }
    fseek(file, 0, 2);
    long l = ftell(file) + 1;
    fseek(file, 0, 0);
    char *block = (char *)malloc(l);
    fread(block, 1, l, file);
    block[l] = 0;
    settings_all(load_func, block);
    fclose(file);
    free(block);
}

setting *setting_find(char const *name) {
    for (setting *s = setting::all_; s != NULL; s = s->next_) {
        if (!strcmp(name, s->name_)) {
            return s;
        }
    }
    return 0;
}


volatile bool interrupted;

void onintr(int) {
    interrupted = true;
}


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
        send_usb(cmd, 2);
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
    send_usb(cmd, n);
    geterrcnt("rest PWM");
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


double lastping;

void handle_ping(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    lastping = now();
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

int n_frames;
sockaddr_in addr_frames;

void handle_camera(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    n_frames = ((cmd_camera const *)hdr)->num_frames;
    addr_frames = *from;
    char buf[256];
    getaddr(from, buf);
    if (verbose) {
        fprintf(stderr, "got camera request from %s for %d\n", buf, n_frames);
    }
}

void handle_power(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
}

void handle_tune(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    cmd_tune *ct = (cmd_tune *)hdr;
    //  I know I'll always have padding space here because the receive buffer is big enough
    ct->value.name[ct->value.slen] = 0;
    setting *s = setting_find(ct->value.name);
    if (s) {
        rest();
        s->set(ct->value.value);
        delay_save = now() + 10;
    }
    else {
        fprintf(stderr, "setting not found in tune: %s\n", ct->value.name);
    }
}

struct gt {
    std::vector<char> packet;
    int n;
};
static void fn_gettune(setting &set, void *g) {
    gt *data = (gt *)g;
    data->n++;
    char buf[200];
    tune_value *tv = (tune_value *)buf;
    tv->value = set;
    tv->slen = strlen(set.name_);
    memcpy(tv->name, set.name_, tv->slen);
    data->packet.insert(data->packet.end(), buf, buf + sizeof(*tv) + tv->slen);
}

void handle_gettune(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {

    gt data;
    data.n = 0;

    cmd_alltune at;
    at.cmd = cAllTune;
    at.cnt = 0;
    data.packet.insert(data.packet.end(), (char *)&at, (char *)&at + sizeof(at));

    settings_all(fn_gettune, &data);

    (*(cmd_alltune *)&data.packet[0]).cnt = data.n;
    fprintf(stderr, "sending alltime size %ld count %d\n", data.packet.size(), data.n);
    do_send(&data.packet[0], data.packet.size(), from);
}

void handle_setminmax(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    cmd_setminmax const *cmd = (cmd_setminmax const *)hdr;
    if (cmd->channel >= 8) {
        fprintf(stderr, "Bad channel in handle_setminmax(): %d\n", cmd->channel);
        return;
    }
    char buf[10];
    buf[0] = (CMD_SETMINMAX << 4) | cmd->channel;
    buf[1] = (cmd->min >> 8) & 0xff;
    buf[2] = cmd->min & 0xff;
    buf[3] = (cmd->max >> 8) & 0xff;
    buf[4] = cmd->max & 0xff;
    send_usb(buf, 5);
}

void handle_setpwm(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    cmd_setpwm const *cmd = (cmd_setpwm const *)hdr;
    if (cmd->channel >= 8) {
        fprintf(stderr, "Bad channel in handle_setpwm(): %d\n", cmd->channel);
        return;
    }
    char buf[10];
    buf[0] = (CMD_SETPWM << 4) | cmd->channel;
    buf[1] = (cmd->value >> 8) & 0xff;
    buf[2] = cmd->value & 0xff;
    send_usb(buf, 3);
}


struct cmd_handler {
    unsigned char cmd;
    size_t min_size;
    void (*handle)(packet_hdr const *hdr, size_t size, sockaddr_in const *from);
};

cmd_handler handlers[] = {
    { cPing, sizeof(cmd_ping) + 1, &handle_ping },
    { cControl, sizeof(cmd_control), &handle_control },
    { cFire, sizeof(cmd_fire), &handle_fire },
    { cReports, sizeof(cmd_reports), &handle_reports },
    { cCamera, sizeof(cmd_camera), &handle_camera },
    { cPower, sizeof(cmd_power), &handle_power },
    { cTune, sizeof(cmd_tune) + 1, &handle_tune },
    { cGetAllTune, sizeof(cmd_gettune) + 1, &handle_gettune },
    { cSetMinMax, sizeof(cmd_setminmax), &handle_setminmax },
    { cSetPWM, sizeof(cmd_setpwm), &handle_setpwm },
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
    float ltarget = 0.25;
    float ctarget = 0;
    float rtarget = 0.25;
    double delay = 0.01;
    switch (g_phase) {
        case 0:
            ltarget = -1;
            ctarget = 0;
            rtarget = -1;
            delay = 0.4 * 1000 / tune_speed;
            break;
        case 1:
            ltarget = 0;
            ctarget = -1;
            rtarget = 0;
            delay = 0.4 * 1000 / tune_speed;
            break;
        case 2:
            ltarget = 1;
            ctarget = 0;
            rtarget = 1;
            delay = 0.4 * 1000 / tune_speed;
            break;
        case 3:
            ltarget = 0;
            ctarget = 1;
            rtarget = 0;
            delay = 0.4 * 1000 / tune_speed;
            break;
        default:
            break;
    }
    if (g_forward > -0.1 && g_forward < 0.1) {
        //  am I turning?
        if (g_turn > 0.1 || g_turn < -0.1) {
            ltarget *= g_forward + std::max(g_turn, 0.f);
            rtarget *= g_forward - std::min(g_turn, 0.f);
        }
        else {
            //  stand still
            ltarget = 0.25;
            rtarget = 0.25;
            ctarget = 0;
        }
    }
    else {
        ltarget *= g_forward + std::max(g_turn, 0.f);
        rtarget *= g_forward - std::min(g_turn, 0.f);
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
    unsigned short t = tune_l_center + ltarget * tune_walk_extent;
    cmd[n++] = (unsigned char)(CMD_LERPPWM << 4) | 0;
    cmd[n++] = (unsigned char)(t >> 8) & 0xff;
    cmd[n++] = t & 0xff;
    cmd[n++] = time;
    t = tune_c_center + ctarget * tune_lift_extent;
    cmd[n++] = (unsigned char)(CMD_LERPPWM << 4) | 1;
    cmd[n++] = (unsigned char)(t >> 8) & 0xff;
    cmd[n++] = t & 0xff;
    cmd[n++] = time;
    t = tune_r_center + rtarget * tune_walk_extent;
    cmd[n++] = (unsigned char)(CMD_LERPPWM << 4) | 2;
    cmd[n++] = (unsigned char)(t >> 8) & 0xff;
    cmd[n++] = t & 0xff;
    cmd[n++] = time;
    send_usb(cmd, n);
    if (verbose) {
        fprintf(stderr, "do_walk %g %g %g for speed %g turn %g phase %d\n", ltarget, ctarget, rtarget, g_forward, g_turn, g_phase);
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
    if (n - lastping > 5) {
        //  make sure there's communication, else stop
        rest();
    }
    if (n - last_errcheck > 0.5) {
        geterrcnt("robot_worker");
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
    if (delay_save != 0 && n > delay_save) {
        setting_save_all("/root/robot/settings.ini");
        delay_save = 0;
        fprintf(stderr, "saved settings\n");
    }
    ctr_forward.set(g_forward);
    setled(lForward, g_forward > 0.1);
    setled(lBackward, g_forward < -0.1);
    ctr_turn.set(g_turn);
    updateled();

    void *data = 0;
    size_t size = 0;
    capture_frame(data, size);
    if (size > 65000) {
        fprintf(stderr, "MJPEG image too big: %Ld\n", (long long)size);
    }
    else {
        if (n_frames) {
            //  re-use existing buffer
            //  There's a bit of an inefficiency here -- I copy data 
            //  from the buffer to dpacket, and then again from dpacket to spacket, 
            //  to send it.
            cmd_frame &cf = *(cmd_frame *)dpacket;
            cf.cmd = cFrame;
            cf.millis = (unsigned short)(now() * 1000);
            memcpy(cf.data, data, size);
            do_send(dpacket, size + sizeof(cf), &addr_frames);
            --n_frames;
        }
    }
}

void init_led() {
    unsigned char cmd[200];
    int n = 0;
    cmd[n++] = (1 << 4) | 2;
    cmd[n++] = 0xff;
    cmd[n++] = (2 << 4) | 2;
    cmd[n++] = 0xff;
    send_usb(cmd, n);
    geterrcnt("init_led()");
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
    signal(SIGHUP, onintr);
    init_socket();
    init_led();
    setting_load_all("/root/robot/settings.ini");

    rest();

    open_dev("/dev/video0", VIDEO_WIDTH, VIDEO_HEIGHT);
    start_capture();

    while (!interrupted) {
        poll_socket();
        robot_worker();
    }

    rest();

    stop_capture();
    close_dev();

    return 0;
}

