
#include <stdio.h>
#include <USBLink.h>
#include <Settings.h>
#include <util.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/fcntl.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/bind.hpp>
#include <ctype.h>
#include <signal.h>

#include <UscServo.h>
#include <Camera.h>
#include <Settings.h>
#include <Image.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Value_Output.H>
#include <FL/Fl_RGB_Image.H>
#include <FL/Fl_Box.H>


#include "ImageListener.h"
#include "cones.h"


#define SAFETY_TIMEOUT 0.5
#define IMAGE_INTERVAL 2


#define CMD_BEEP (0x10 | 0x01)          //  cmd 1, sizeix 1
#define CMD_MOTOR_SPEEDS (0x20 | 0x02)  //  cmd 2, sizeix 2

#define RET_CURRENT_VALUES (0x40 | 0x08 | 0x02) //  ret 4, RET, sizeix 2
#define RET_COUNTER_VALUES (0x50 | 0x08 | 0x05) //  ret 5, RET, sizeix 5

boost::shared_ptr<Module> usb_;
boost::shared_ptr<boost::thread> usbThread_;
unsigned char lastSeq_;
unsigned char nextSeq_;
bool safeToDrive_ = true;
bool requireSafety_ = false;

bool thresholdSize_ = 16 * 16;
bool foundcone = false;
float conesteer = 0;

std::string capturePath_;

UscServo * uscServo_;

short servosteer[4] = { 0, 0, 0, 0 };
unsigned short servocenters[4] = { 6000, 6000, 6000, 6000 };


unsigned char lastmotorpower[2] = { 0x80, 0x80 };
unsigned char motorpower[2] = { 0x80, 0x80 };
unsigned char currents[2];
unsigned short counters[4];
bool running = true;


class Behavior {
public:
    virtual void step() = 0;
    virtual bool complete() = 0;
    virtual ~Behavior() {}
};


#define TICKS_TO_INCHES 0.0155

#define PWR_LEFT 0
#define PWR_RIGHT 1

//  servo 0 turns left on negative, leftrear wheel
//  servo 1 turns left on negative, rightrear wheel
//  servo 2 turns right on negative, leftfront wheel
//  servo 3 turns right on negative, rightfront wheel

//  counter 3 goes backwards, rightrear wheel
//  counter 2 goes forwards, leftrear wheel
//  counter 1 goes backwards, rightfront wheel
//  counter 0 goes forwards, leftfront wheel

class StraightBehavior : public Behavior {
public:
    unsigned short baseline[4];
    float ratePerSecond;
    double inittime;
    double snaptime[4];
    double baseTravelL; double baseTravelR;
    double curTravelL; double curTravelR;
    double toDistance;
    float leftScale;
    float rightScale;
    bool complete() {
        if (ratePerSecond > 0) {
            //std::cerr << std::max(baseTravelR + curTravelR, baseTravelL + curTravelL) * TICKS_TO_INCHES << " in" << std::endl << std::flush;
            return toDistance <= std::max(baseTravelR + curTravelR, baseTravelL + curTravelL) * TICKS_TO_INCHES;
        }
            //std::cerr << std::min(baseTravelR + curTravelR, baseTravelL + curTravelL) * TICKS_TO_INCHES << " in" << std::endl << std::flush;
        return toDistance >= std::min(baseTravelR + curTravelR, baseTravelL + curTravelL) * TICKS_TO_INCHES;
    }
    StraightBehavior(float speed, double distance) { //  speed in inches per second
        leftScale = 1;
        rightScale = 1;
        toDistance = distance;
        baseTravelR = 0;
        baseTravelL = 0;
        curTravelL = 0;
        curTravelL = 0;
        inittime = read_clock();
        snaptime[0] = snaptime[1] = snaptime[2] = snaptime[3] = inittime;
        ratePerSecond = speed;
        memcpy(baseline, counters, 8);
        memset(servosteer, 0, sizeof(servosteer));
        float s = speed * 2;
        if (s > 127) {
            s = 127;
        }
        else if (s < -127) {
            s = -127;
        }
        unsigned char spd = 0x80 + (char)s;
        motorpower[0] = motorpower[1] = spd;
    }
    virtual void step() {
        double now = read_clock();
        double deltaTime = now - inittime;
        if (deltaTime > 0.05) {
            short deltaLR = (counters[2] - baseline[2]);
            short deltaLF = (counters[0] - baseline[0]);
            short deltaRR = -(counters[3] - baseline[3]);
            short deltaRF = -(counters[1] - baseline[1]);
            float leftTPS = deltaLR / (now - snaptime[2]) * TICKS_TO_INCHES * leftScale;
            float rightTPS = deltaRR / (now - snaptime[3]) * TICKS_TO_INCHES * rightScale;
            if (ratePerSecond > 0) {
                if (deltaLF < deltaLR) {
                    leftTPS = deltaLF / (now - snaptime[0]) * TICKS_TO_INCHES * leftScale;
                    curTravelL = deltaLF;
                }
                else {
                    curTravelL = deltaLR;
                }
                if (deltaRF < deltaRR) {
                    rightTPS = deltaRF / (now - snaptime[1]) * TICKS_TO_INCHES * rightScale;
                    curTravelR = deltaRF;
                }
                else {
                    curTravelR = deltaRR;
                }
                if (leftTPS < ratePerSecond) {
                    if (motorpower[PWR_LEFT] < 0xff) {
                        motorpower[PWR_LEFT]++;
                    }
                }
                else if (leftTPS > ratePerSecond) {
                    if (motorpower[PWR_LEFT] > 0x80) {
                        motorpower[PWR_LEFT]--;
                    }
                }
                if (rightTPS < ratePerSecond) {
                    if (motorpower[PWR_RIGHT] < 0xff) {
                        motorpower[PWR_RIGHT]++;
                    }
                }
                else if (rightTPS > ratePerSecond) {
                    if (motorpower[PWR_RIGHT] > 0x80) {
                        motorpower[PWR_RIGHT]--;
                    }
                }
            }
            else {
                if (deltaLF > deltaLR) {
                    leftTPS = deltaLF / (now - snaptime[0]) * TICKS_TO_INCHES * leftScale;
                    curTravelL = deltaLF;
                }
                else {
                    curTravelL = deltaLR;
                }
                if (deltaRF > deltaRR) {
                    rightTPS = deltaRF / (now - snaptime[1]) * TICKS_TO_INCHES * rightScale;
                    curTravelR = deltaRF;
                }
                else {
                    curTravelR = deltaRR;
                }
                if (leftTPS < ratePerSecond) {
                    if (motorpower[PWR_LEFT] < 0xff) {
                        motorpower[PWR_LEFT]++;
                    }
                }
                else if (leftTPS > ratePerSecond) {
                    if (motorpower[PWR_LEFT] > 0x80) {
                        motorpower[PWR_LEFT]--;
                    }
                }
                if (rightTPS < ratePerSecond) {
                    if (motorpower[PWR_RIGHT] < 0xff) {
                        motorpower[PWR_RIGHT]++;
                    }
                }
                else if (rightTPS > ratePerSecond) {
                    if (motorpower[PWR_RIGHT] > 0x80) {
                        motorpower[PWR_RIGHT]--;
                    }
                }
            }
            if ((deltaLR > 20000 || deltaLR < -20000) 
                || (deltaLF > 20000 || deltaLF < -20000)) {
                baseline[2] = counters[2];
                snaptime[2] = now - 0.001;
                baseline[0] = counters[0];
                snaptime[0] = now - 0.001;
                baseTravelL += curTravelL;
                curTravelL = 0;
            }
            if ((deltaRR > 20000 || deltaRR < -20000)
                || (deltaRF > 20000 || deltaRF < -20000)) {
                baseline[3] = counters[3];
                snaptime[3] = now - 0.001;
                baseline[1] = counters[1];
                snaptime[1] = now - 0.001;
                baseTravelR += curTravelR;
                curTravelR = 0;
            }
            inittime = now;
        }
    }
};

class TurnBehavior : public StraightBehavior {
public:
    float direction;
    TurnBehavior(float speed, float dir, float distance) :
        StraightBehavior(speed, distance) {
        setdir(dir);
    }
    void setdir(float dir) {
        direction = dir;
        servosteer[0] = (short)(800 * dir);
        servosteer[1] = (short)(800 * dir);
        servosteer[2] = -(short)(800 * dir);
        servosteer[3] = -(short)(800 * dir);
        if (dir < 0) {
            leftScale = 1 - dir * 0.3;
        }
        else {
            rightScale = 1 + dir * 0.3;
        }
    }
};

class ConeBehavior : public TurnBehavior {
public:
    double phasetime;
    int phase;
    ConeBehavior(float speed, float distance) :
        TurnBehavior(speed, 0, distance) {
        phasetime = read_clock();
        phase = -0.8;
    }
    virtual void step() {
        if (foundcone) {
            setdir(conesteer);
        }
        else {
            double now = read_clock();
            if (now - phasetime > 2.5) {
                phase = -phase;
                phasetime += 5;
                std::cerr << "wind " << phase << std::endl;
            }
            setdir(phase);
        }
        TurnBehavior::step();
    }
};

void doquit(int) {
    running = false;
}

void setservo(int i, short steer) {
    uscServo_->steer(i, steer);
}

void setallservos() {
    for (int i = 0; i != 4; ++i) {
        setservo(i, servosteer[i]);
    }
}

void setpower(USBLink *link) {
    if (safeToDrive_) {
        if (motorpower[0] > lastmotorpower[0]) {
            lastmotorpower[0] += 1;
        }
        if (motorpower[0] < lastmotorpower[0]) {
            lastmotorpower[0] -= 1;
        }
        if (motorpower[1] > lastmotorpower[1]) {
            lastmotorpower[1] += 1;
        }
        if (motorpower[1] < lastmotorpower[1]) {
            lastmotorpower[1] -= 1;
        }
    }
    unsigned char pwr[2] = { lastmotorpower[0], lastmotorpower[1] };
    unsigned char buf[4] = {
        0,
        CMD_MOTOR_SPEEDS,
        (unsigned char)(safeToDrive_ ? pwr[0] : 0x80),
        (unsigned char)(safeToDrive_ ? pwr[1] : 0x80)
        };
    link->raw_send(buf, 4);
}

void usb_thread_fn() {
    sched_param parm = { .sched_priority = 25 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "USBLink::thread_fn(): pthread_setschedparam(): " << err << std::endl;
    }
    USBLink *link = usb_->cast_as<USBLink>();

    double steer_time = read_clock();
    while (true) {

        link->step();

        while (true) {
            //  drain receive queue
            size_t sz = 0;
            unsigned char const *ptr = link->begin_receive(sz);
            if (!ptr) {
                link->end_receive(0);
                break;
            }
            unsigned char const *end = ptr + sz;
            lastSeq_ = *ptr;
            ++ptr;
            while (ptr < end) {
                switch (*ptr) {
                    case RET_CURRENT_VALUES:
                        memcpy(currents, ptr+1, 2);
                        ptr += 3;
                        break;
                    case RET_COUNTER_VALUES:
                        memcpy(counters, ptr+1, 8);
                        ptr += 9;
                        break;
                    default:
                        link->end_receive(sz);
                        throw std::runtime_error("unknown command received from USB: "
                            + hexnum(*ptr));
                }
            }
            link->end_receive(sz);
        }

        double now = read_clock();

        if (now - steer_time > 0.05) {
            setallservos();
            setpower(link);
            steer_time = now;
        }
        usleep(3000);
    }
}

int safetyid = -1;
int numimages = 0;
boost::shared_ptr<boost::thread> safetyThread_;
double lastSafety_;
double lastImage_;
double lastCone_;


boost::shared_ptr<Module> camera_;
boost::shared_ptr<ImageListener> listener_;
boost::shared_ptr<Image> boxImage_;

Fl_RGB_Image *flImage_;
Fl_Box *flImageBox_;


Behavior *behave_;

void idle_func() {
    camera_->step();
    usleep(5000);
    double now = read_clock();
    if (!running) {
        std::cerr << now << ", SIGINT received; quitting" << std::endl;
        exit(1);
        return;
    }
    if (requireSafety_ && (now - lastSafety_ > SAFETY_TIMEOUT)) {
        if (safeToDrive_) {
            safeToDrive_ = false;
            std::cerr << now << ", safety timeout" << std::endl;
        }
        lastSafety_ = now + 10;
    }
    boost::shared_ptr<Image> image_(listener_->image_);
    if (now - lastCone_ > 0.03 && !!image_) {
        lastCone_ = now;
        RPixmap pm(image_, false);
        Area out(0, pm.height * 0.25, pm.width, pm.height * 0.75);
        float size = 0;
        foundcone = find_a_cone(pm, out, conesteer, size, false);
        if (foundcone && size > thresholdSize_) {
            pm.frame_rect(out, Color(255, 255, 255));
        }
        delete flImage_;
        boxImage_ = image_;
        flImage_ = new Fl_RGB_Image(
            (unsigned char const *)image_->bits(FullBits),
            image_->width(),
            image_->height(),
            Image::BytesPerPixel);
        flImageBox_->image(flImage_);
        flImageBox_->damage(0xff);
        flImageBox_->redraw();
        if (now - lastImage_ > IMAGE_INTERVAL) {
            char name[1024];
            ++numimages;
            sprintf(name, "%s/image_%04d.jpg", capturePath_.c_str(), numimages);
            pm.save_jpg(name);
            lastImage_ = now;
        }
    }
    //  think here
    if (behave_) {
        behave_->step();
        if (behave_->complete()) {
            std::cerr << "complete" << std::endl;
            exit(1);
        }
    }
}

void open_servos(boost::shared_ptr<Settings> const &set) {
    uscServo_ = new UscServo(0x1ffb, 0x0089, servocenters);
}


static void safety_incoming_line(char const *line) {
    double now = read_clock();
    if (!strcmp(line, "1")) {
        if (!safeToDrive_) {
            std::cerr << now << ", safe to drive" << std::endl;
        }
        safeToDrive_ = true;
        lastSafety_ = now;
    }
    else if (!strcmp(line, "0")) {
        if (safeToDrive_) {
            std::cerr << now << ", unsafe to drive!" << std::endl;
        }
        safeToDrive_ = false;
    }
    else {
        std::cout << now << ", INCOMING: " << line << std::endl;
    }
}

static void run_safety() {
    char line[1024];
    int ptr = 0;
    lastSafety_ = read_clock();
    while (!safetyThread_->interruption_requested()) {
        int i = read(safetyid, &line[ptr], sizeof(line)-1-ptr);
        if (i < 1) {
            if (errno == EAGAIN) {
                continue;
            }
            std::cerr << read_clock() << ", Error in xbee read(); exiting." << std::endl;
            break;
        }
        ptr += i;
        if (ptr == sizeof(line)-1 || line[ptr-1] == '\n') {
            while (ptr > 0 && isspace(line[ptr-1])) {
                --ptr;
            }
            line[ptr] = 0;
            safety_incoming_line(line);
            ptr = 0;
        }
    }
    ::close(safetyid);
    safetyid = -1;
}

void open_safety(boost::shared_ptr<Settings> const &xbee) {
    std::string device(xbee->get_value("device")->get_string());
    if (safetyid != -1) {
        ::close(safetyid);
        safetyid = -1;
    }
    safetyid = open(device.c_str(), O_RDWR);
    struct termios tio;
    tcgetattr(safetyid, &tio);
    cfmakeraw(&tio);
    cfsetspeed(&tio, B9600);
    tio.c_cc[VMIN] = 1;
    tio.c_cc[VTIME] = 1;
    tio.c_lflag &= ~(ICANON);
    int e = tcsetattr(safetyid, TCSANOW, &tio);
    if (e < 0) {
        std::cerr << "warning: could not tcsetattr(" << device << ")" << std::endl;
    }
    safeToDrive_ = false;
    safetyThread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(run_safety)));
}

void open_capture() {
    char buf[256];
    time_t t;
    time(&t);
    strftime(buf, 256, "/var/robot/%Y-%m-%d.%H.%M.%S", localtime(&t));
    mkdir(buf, 0777);
    struct stat st;
    if (stat(buf, &st) < 0 || !S_ISDIR(st.st_mode)) {
        perror(buf);
        exit(1);
    }
    capturePath_ = buf;
}

int main() {
    signal(SIGINT, doquit);
    boost::shared_ptr<Settings> set(Settings::load("robot.json"));
    open_capture();
    open_servos(set);
    requireSafety_ = false;
    if (set->has_name("safety") && set->get_value("safety")->get_long()) {
        open_safety(set->get_value("xbee"));
        requireSafety_ = true;
    }
    usb_ = USBLink::open(set, boost::shared_ptr<Logger>());
    usbThread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(usb_thread_fn)));

    boost::shared_ptr<Module> camera(Camera::open(set->get_value("camera")));
    boost::shared_ptr<Property> image(camera->get_property_named("image"));
    boost::shared_ptr<ImageListener> image_listener(new ImageListener(image));
    image->add_listener(image_listener);

    camera_ = camera;
    listener_ = image_listener;

    Fl_Window *w = new Fl_Double_Window(800, 500, "Navigate");

    flImage_ = nullptr;
    flImageBox_ = new Fl_Box(0, 0, 432, 240);

    w->end();
    w->show();

    //behave_ = new StraightBehavior(-10, -30);
    behave_ = new ConeBehavior(5, 1000);

    Fl::set_idle(idle_func);
    Fl::run();
    delete uscServo_;
    return 0;
}

