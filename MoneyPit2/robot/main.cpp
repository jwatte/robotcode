
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

#include <UscServo.h>
#include <Camera.h>
#include <Settings.h>
#include <Image.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Output.H>


#include "ImageListener.h"



#define CMD_BEEP (0x10 | 0x01)          //  cmd 1, sizeix 1
#define CMD_MOTOR_SPEEDS (0x20 | 0x02)  //  cmd 2, sizeix 2

#define RET_CURRENT_VALUES (0x40 | 0x08 | 0x02) //  ret 4, RET, sizeix 2
#define RET_COUNTER_VALUES (0x50 | 0x08 | 0x05) //  ret 5, RET, sizeix 5

boost::shared_ptr<Module> usb_;
boost::shared_ptr<boost::thread> usbThread_;
unsigned char lastSeq_;
unsigned char nextSeq_;
bool safeToDrive_ = true;

UscServo * uscServo_;

short servosteer[4] = { 0, 0, 0, 0 };
unsigned short servocenters[4] = { 6000, 6000, 6000, 6000 };

unsigned char motorpower[2] = { 0x80, 0x80 };
unsigned char currents[2];
unsigned short counters[4];

void setservo(int i, short steer) {
    uscServo_->steer(i, steer);
}

void setallservos() {
    for (int i = 0; i != 4; ++i) {
        setservo(i, servosteer[i]);
    }
}

void setpower(USBLink *link) {
    unsigned char buf[4] = {
        0,
        CMD_MOTOR_SPEEDS,
        motorpower[0],
        motorpower[1]
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


boost::shared_ptr<Module> camera_;
boost::shared_ptr<ImageListener> listener_;

void idle_func() {
    usleep(5000);
    camera_->step();
    //  think here
}

void open_servos(boost::shared_ptr<Settings> const &set) {
    uscServo_ = new UscServo(0x1ffb, 0x0089, servocenters);
}

int safetyid = -1;
boost::shared_ptr<boost::thread> safetyThread_;


static void safety_incoming_line(char const *line) {
    std::cout << "INCOMING: " << line << std::endl << std::flush;
}

static void run_safety() {
    char line[1024];
    int ptr = 0;
    while (!safetyThread_->interruption_requested()) {
        int i = read(safetyid, &line[ptr], sizeof(line)-1-ptr);
        if (i < 1) {
            if (errno == EAGAIN) {
                continue;
            }
            std::cerr << "Error in xbee read(); exiting." << std::endl;
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

int main() {
    boost::shared_ptr<Settings> set(Settings::load("robot.json"));
    open_servos(set);
    if (set->has_name("safety") && set->get_value("safety")->get_long()) {
        open_safety(set->get_value("xbee"));
    }
    usb_ = USBLink::open(set, boost::shared_ptr<Logger>());
    usbThread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(usb_thread_fn)));

    boost::shared_ptr<Module> camera(Camera::open(set->get_value("camera")));
    boost::shared_ptr<Property> image(camera->get_property_named("image"));
    boost::shared_ptr<ImageListener> image_listener(new ImageListener(image));
    image->add_listener(image_listener);

    camera_ = camera;
    listener_ = image_listener;

    Fl_Window *w = new Fl_Window(800, 500, "Navigate");

    w->end();
    w->show();

    Fl::set_idle(idle_func);
    Fl::run();
    delete uscServo_;
    return 0;
}

