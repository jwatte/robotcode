#include "../lib/defs.h"

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
#include <string>
#include <memory>
#include <vector>

#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Counter.H>
#include <FL/Fl_JPEG_Image.H>
#include <FL/Fl_Box.H>



class ControlWindow : public Fl_Double_Window {
public:
    ControlWindow();
    ~ControlWindow();
    void set_mjpeg(void const *data, size_t size);
    std::vector<unsigned char> buf_;
    Fl_JPEG_Image *img_;
    Fl_Box *imgBox_;
    Fl_Box *roboName_;
    Fl_Box *roboAddr_;
};


static const unsigned char huff_data[] = {
255,196,1,162,0,0,1,5,1,1,1,1,1,1,0,0,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,
11, 1,0,3,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,1,2,3,4,5,6,7,8,9,10,11,16,0,2,
1,3,3,2,4,3,5,5,4,4,0,0,1,125,1,2,3,0,4,17,5,18,33,49,65,6,19,81,97, 7,
34,113,20,50,129,145,161,8,35,66,177,193,21,82,209,240,36,51,98,114,
130,9,10,22,23,24,25,26,37,38,39,40,41,42,52,53,54,55,56,57,58,67,68,
69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,
104,105,106,115,116,117,118,119,120,121,122,131,132,133,134,135,136,
137,138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,
168,169,170,178,179,180,181,182,183,184,185,186,194,195,196,197,198,
199,200,201,202,210,211,212,213,214,215,216,217,218,225,226,227,228,
229,230,231,232,233,234,241,242,243,244,245,246,247,248,249,250,17,0,2,
1,2,4,4,3,4,7,5,4,4,0,1,2,119,0,1,2,3,17,4,5,33,49,6,18,65,81,7,97,113,
19,34,50,129,8,20,66,145,161,177,193,9,35,51,82,240,21,98,114,209,10,
22,36,52,225,37,241,23,24,25,26,38,39,40,41,42,53,54,55,56,57,58,67,68,
69,70,71,72,73,74,83,84,85,86,87,88,89,90,99,100,101,102,103,104,105,
106,115,116,117,118,119,120,121,122,130,131,132,133,134,135,136,137,
138,146,147,148,149,150,151,152,153,154,162,163,164,165,166,167,168,
169,170,178,179,180,181,182,183,184,185,186,194,195,196,197,198,199,
200,201,202,210,211,212,213,214,215,216,217,218,226,227,228,229,230,
231,232,233,234,242,243,244,245,246,247,248,249,250
};
static const unsigned int huff_size = sizeof(huff_data);

char const *myname;
bool verbose = true;
ControlWindow *mainWindow;

double gotrobo = 0;
sockaddr_in roboaddr;

char robo_name[256] = "none";
char robo_addr[256] = "0.0.0.0";

volatile bool interrupted;

void onintr(int) {
    interrupted = true;
}


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


void set_robo_name(char const *name, char const *addr) {
    strcpy(robo_name, name);
    strcpy(robo_addr, addr);
    mainWindow->roboName_->redraw();
    mainWindow->roboAddr_->redraw();
    mainWindow->redraw();
}

void handle_pong(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    char buf[256];
    memcpy(buf, &((cmd_pong *)hdr)[1], ((cmd_pong *)hdr)->slen);
    buf[((cmd_pong *)hdr)->slen] = 0;
    char addr[256];
    getaddr(from, addr);
    if (verbose) {
        fprintf(stderr, "got pong from %s (%s)\n", buf, addr);
    }
    //  don't replace with another robot
    if (!gotrobo || !strcmp(robo_addr, addr)) {
        set_robo_name(buf, addr);
        gotrobo = now();
        roboaddr = *from;
    }
}

PacketHandler *alltune_handler;

void handle_alltune(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    if (alltune_handler) {
        alltune_handler->onPacket(hdr, size, from);
    }
    else if (verbose) {
        fprintf(stderr, "got cAllTune without handler\n");
    }
}

void handle_frame(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
    if (verbose) {
        char buf[256];
        getaddr(from, buf);
        fprintf(stderr, "got frame from %s\n", buf);
    }
    cmd_frame const *cf = (cmd_frame const *)hdr;
    mainWindow->set_mjpeg(cf->data, size - sizeof(*cf));
}

struct cmd_handler {
    unsigned char cmd;
    size_t min_size;
    void (*handle)(packet_hdr const *hdr, size_t size, sockaddr_in const *from);
};

cmd_handler handlers[] = {
    { cPong, sizeof(cmd_pong), &handle_pong },
    { cAllTune, sizeof(cmd_alltune), &handle_alltune },
    { cFrame, sizeof(cmd_frame) + 1000, &handle_frame },
};

void dispatch_packet(void const *packet, size_t size, sockaddr_in const *from) {
    char ipaddr[30];
    getaddr(from, ipaddr);
    if (verbose) {
        fprintf(stderr, "got packet from %s\n", ipaddr);
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

void recv_one() {
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
    if (select(sock + 1, &fds, 0, 0, &tv) > 0) {
        recv_one();
    }
}

float g_lastForward;
float g_lastTurn;

void control(float forward, float turn) {
    g_lastForward = forward;
    g_lastTurn = turn;
    if (gotrobo) {
        cmd_control cc;
        cc.cmd = cControl;
        cc.speed = (short)(forward * 4096);
        cc.rate = (short)(turn * 4096);
        do_send(&cc, sizeof(cc), &roboaddr);
        fprintf(stderr, "do_send(%d, %d)\n", cc.speed, cc.rate);
    }
}


class Activity {
public:
    Activity(Activity *below = 0) : below_(below) {}
    Activity *below_;
    virtual void start() {
        if (below_) {
            below_->start();
        }
    }
    virtual void step() {
        if (below_) {
            below_->step();
        }
    }
    virtual void stop() {
        if (below_) {
            below_->stop();
        }
    }
};

Activity *cur_activity;

void set_activity(Activity *act) {
    if (cur_activity) {
        cur_activity->stop();
    }
    cur_activity = act;
    if (cur_activity) {
        cur_activity->start();
    }
    mainWindow->redraw();
}

double last_request = 0;

void commander_worker() {
    double n = now();
    if (n - last_request > 0.5) {
        //  request reports
        //  request images
        last_request = n;
    }
}


void main_idle(void *) {
    poll_socket();
    cur_activity->step();
}

class IdleActivity : public Activity {
public:
    void start() {
        Activity::start();
        last_control = 0;
        last_ping = 0;
        last_frames = 0;
    }
    void step() {
        Activity::step();
        double n = now();
        if (gotrobo && n - gotrobo > 5) {
            //  lost connection!
            gotrobo = 0;
            set_robo_name("none", "0.0.0.0");
        }
        if (n - last_control > 1) {
            control(g_lastForward, g_lastTurn);
            last_control = n;
        }
        //  ping every 2 seconds when connected, and 0.5 second when not connected
        if ((n - last_ping > 2) || (!gotrobo && (n - last_ping > 0.5))) {
            char buf[256];
            cmd_pong *cp = (cmd_pong *)buf;
            cp->cmd = cPing;
            cp->slen = strlen(myname);
            memcpy(&cp[1], myname, cp->slen);
            struct sockaddr_in bc;
            memset(&bc, 0, sizeof(bc));
            memset(&bc.sin_addr, 0xff, sizeof(bc.sin_addr));
            bc.sin_port = htons(CONTROL_PORT);
            do_send(buf, sizeof(*cp) + cp->slen, &bc);
            last_ping = n;
        }
        if (gotrobo && (n - last_frames > 1)) {
            cmd_camera cc;
            cc.cmd = cCamera;
            cc.num_frames = 50;
            fprintf(stderr, "sending cCamera\n");
            do_send(&cc, sizeof(cc), &roboaddr);
            last_frames = n;
        }
    }
    void stop() {
        Activity::stop();
    }
    double last_control;
    double last_ping;
    double last_frames;
};

IdleActivity idle;

class DisplayActivity : public Activity {
public:
    DisplayActivity(Activity *below = 0) :
        Activity(below)
    {
    }
};

DisplayActivity displayIdle(&idle);



void quit_program(Fl_Widget *) {
    if (Fl::event() == FL_SHORTCUT && Fl::event_key() == FL_Escape) {
        return; //  don't quit on Escape
    }
    fprintf(stderr, "Quit program received.\n");
    exit(0);
}

class TuneActivity : public Activity, public PacketHandler {
public:
    TuneActivity(Activity *below) :
        Activity(below)
    {
    }
    static void exitb_cb(Fl_Widget *) {
        set_activity(&displayIdle);
    }
    void start() {
        fprintf(stderr, "TuneActivity::start()\n");
        Activity::start();
        last_poll = 0;
        alltune_handler = this;
        exitb = new Fl_Button(120, 10, 100, 24, "<< Done");
        exitb->callback(exitb_cb);
        mainWindow->add(exitb);
        exitb->redraw();
    }
    void step() {
        Activity::step();
        double n = now();
        if (n - last_poll > 1) {
            send_poll();
            last_poll = n;
        }
    }
    void stop() {
        alltune_handler = 0;
        counters_.clear();
        delete exitb;
        Activity::stop();
    }

    static void tweak_tune(Fl_Widget *wig, void *arg) {
        if (gotrobo) {
            char buf[300];
            cmd_tune *ct = (cmd_tune *)buf;
            ct->cmd = cTune;
            ct->value.value = static_cast<Fl_Counter *>(wig)->value();
            ct->value.slen = strlen((char *)arg);
            memcpy(ct->value.name, arg, ct->value.slen);
            do_send(buf, sizeof(*ct) + ct->value.slen, &roboaddr);
        }
    }

    void onPacket(packet_hdr const *hdr, size_t size, sockaddr_in const *from) {
        cmd_alltune const *at = (cmd_alltune const *)hdr;
        tune_value const *tv = at->values;
        for (int i = 0; i < at->cnt; ++i) {
            char name[256];
            memcpy(name, tv->name, tv->slen);
            name[tv->slen] = 0;
            Fl_Counter *ctr = counters_[name].get();
            if (!ctr) {
                std::unordered_map<std::string, std::unique_ptr<Fl_Counter>>::iterator ptr(
                    counters_.find(name));
                (*ptr).second.reset(new Fl_Counter(120, 10 + int(32 * counters_.size()),
                    150, 24, (*ptr).first.c_str()));
                (*ptr).second->callback(tweak_tune, (void *)(*ptr).first.c_str());
                (*ptr).second->lstep(100);
                mainWindow->add((*ptr).second.get());
                (*ptr).second->align(FL_ALIGN_LEFT);
                ctr = (*ptr).second.get();
            }
            ctr->value(tv->value);
            ctr->redraw();
            tv = (tune_value const *)(((char *)&tv[1]) + tv->slen);
        }
        mainWindow->redraw();
    }
    void send_poll() {
        if (gotrobo) {
            cmd_gettune cgt;
            cgt.cmd = cGetAllTune;
            do_send(&cgt, sizeof(cgt), &roboaddr);
        }
    }

    std::unordered_map<std::string, std::unique_ptr<Fl_Counter>> counters_;
    double last_poll;
    Fl_Button *exitb;
};

TuneActivity tuneActivity(&displayIdle);

void cb_tune(Fl_Widget *btn) {
    fprintf(stderr, "cb_tune()\n");
    set_activity(&tuneActivity);
}


static void cb_forward(Fl_Widget *) {
    control(1, 0);
}

static void cb_fleft(Fl_Widget *) {
    control(1, -1);
}

static void cb_fright(Fl_Widget *) {
    control(1, 1);
}

static void cb_backward(Fl_Widget *) {
    control(-1, 0);
}

static void cb_stop(Fl_Widget *) {
    control(0, 0);
}

ControlWindow::ControlWindow() :
    Fl_Double_Window(10, 30, 1200, 750, "Robot Control")
{
    begin();
    Fl_Button *tune = new Fl_Button(10, 10+600, 100, 24, "Tune");
    tune->callback(cb_tune);
    Fl_Button *forward = new Fl_Button(42, 42+600, 32, 32, "@8->");
    forward->callback(cb_forward);
    Fl_Button *fleft = new Fl_Button(10, 42+600, 32, 32, "@7->");
    fleft->callback(cb_fleft);
    Fl_Button *stop = new Fl_Button(42, 74+600, 32, 32, "@3+");
    stop->callback(cb_stop);
    Fl_Button *fright = new Fl_Button(74, 42+600, 32, 32, "@9->");
    fright->callback(cb_fright);
    Fl_Button *down = new Fl_Button(42, 106+600, 32, 32, "@2->");
    down->callback(cb_backward);
    img_ = 0;
    imgBox_ = new Fl_Box(330, 100, VIDEO_WIDTH, VIDEO_HEIGHT, "");
    roboName_ = new Fl_Box(240, 10, 100, 24, robo_name);
    roboName_->align(FL_ALIGN_INSIDE);
    roboAddr_ = new Fl_Box(330, 10, 100, 24, robo_addr);
    roboAddr_->align(FL_ALIGN_INSIDE);
    end();
    callback(&quit_program);
}

ControlWindow::~ControlWindow()
{
}

void ControlWindow::set_mjpeg(void const *data, size_t size) {
    size_t need = size + huff_size;
    if (need > buf_.size()) {
        buf_.resize(need + 32768);
    }
    unsigned char const *ptr = (unsigned char const *)data;
    unsigned char const *end = ptr + size;
    // insert the Huffman table
    while (ptr < end) {
        if (ptr[0] == 0xff && ptr[1] == 0xda) {
            break;
        }
        ++ptr;
    }
    if (ptr == end) {
        fprintf(stderr, "Invalid MJPEG data in ControlWindow::set_mjpeg()\n");
        return;
    }
    size_t offset = ptr - (unsigned char const *)data;
    memcpy(&buf_[0], data, offset);
    memcpy(&buf_[offset], huff_data, huff_size);
    memcpy(&buf_[offset + huff_size], ptr, end - ptr);

    imgBox_->image(0);
    delete img_;
    img_ = new Fl_JPEG_Image(0, &buf_[0]);
    imgBox_->image(img_);
    imgBox_->redraw();
}


int main(int argc, char const *argv[]) {
    if (argc > 1) {
        myname = argv[1];
        ++argv;
        --argc;
    }
    else {
        myname = getenv("CMDNAME");
    }
    if (!myname) {
        fprintf(stderr, "specify name as first argument, or CMDNAME environment variable.\n");
        exit(1);
    }
    if (strlen(myname) > 64) {
        fprintf(stderr, "too long commander name: %d\n", (int)strlen(myname));
        exit(1);
    }
    if (strlen(myname) < 1) {
        fprintf(stderr, "commander name cannot be empty.\n");
        exit(1);
    }

    signal(SIGINT, onintr);
    init_socket();

    (mainWindow = new ControlWindow())->show();
    set_activity(&displayIdle);
    Fl::add_idle(main_idle, 0);
    Fl::run();

    return 0;
}

