
#include <stdio.h>
#include <USBLink.h>
#include <Settings.h>
#include <util.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <boost/lexical_cast.hpp>

#include <UscServo.h>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Value_Slider.H>
#include <FL/Fl_Button.H>
#include <FL/Fl_Value_Output.H>





#define CMD_BEEP (0x10 | 0x01)          //  cmd 1, sizeix 1
#define CMD_MOTOR_SPEEDS (0x20 | 0x02)  //  cmd 2, sizeix 2
//#define CMD_SERVO_TIMES (0x30 | 0x05)   //  cmd 3, sizeix 5
//#define CMD_WRITE_SERVOS (0x30 | 0x00)  //  cmd 3, sizeix 0

#define RET_CURRENT_VALUES (0x40 | 0x08 | 0x02) //  ret 4, RET, sizeix 2
#define RET_COUNTER_VALUES (0x50 | 0x08 | 0x05) //  ret 5, RET, sizeix 5

boost::shared_ptr<Module> usb_;
boost::shared_ptr<boost::thread> usbThread_;
unsigned char lastSeq_;
unsigned char nextSeq_;

UscServo * uscServo_;

short servosteer[4] = { 0, 0, 0, 0 };
unsigned short servocenters[4] = { 6000, 6000, 6000, 6000 };

unsigned char motorpower[2] = { 0x80, 0x80 };
unsigned char currents[2];
unsigned short counters[4];

Fl_Valuator * valueDisplay_[4];

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
                        for (int i = 0; i != 4; ++i) {
                            valueDisplay_[i]->value(counters[i]);
                            valueDisplay_[i]->redraw();
                        }
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


void idle_func() {
    usleep(5000);
}

void slider_cb(Fl_Widget *vs, void *ii) {
    int i = (int)((size_t)ii & 0x3);
    double d = static_cast<Fl_Valuator *>(vs)->value();
    servosteer[i] = (short)d;
    setservo(i, servosteer[i]);
}

void speed_cb(Fl_Widget *vs, void *ii) {
    int i = (int)((size_t)ii & 0x3);
    double d = static_cast<Fl_Valuator *>(vs)->value();
    if (i == 0 || i == 3) {
        motorpower[0] = motorpower[1] = (unsigned char)(128 - d);
    }
    else if (i == 1) {
        motorpower[0] = (unsigned char)(d + 128);
    }
    else if (i == 2) {
        motorpower[1] = (unsigned char)(d + 128);
    }
}

void open_servos(boost::shared_ptr<Settings> const &set) {
    uscServo_ = new UscServo(0x1ffb, 0x0089, servocenters);
}

int main() {
    boost::shared_ptr<Settings> set(Settings::load("robot.json"));
    open_servos(set);
    usb_ = USBLink::open(set, boost::shared_ptr<Logger>());
    usbThread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(usb_thread_fn)));

    Fl_Window *w = new Fl_Window(800, 500, "Control");
    for (int i = 0; i < 4; ++i) {
        Fl_Value_Slider *vs = new Fl_Value_Slider(10, 10+35*i, 300, 25);
        vs->type(FL_HORIZONTAL);
        vs->bounds(-1000, 1000);
        vs->value(0);
        vs->callback(slider_cb, (void *)(size_t)i);
    }

    Fl_Value_Slider *vs = new Fl_Value_Slider(350, 10, 30, 300);
    vs->type(FL_VERTICAL);
    vs->bounds(-127, 127);
    vs->value(0);
    vs->callback(speed_cb, (void *)0);

    for (int i = 0; i < 4; ++i) {
        Fl_Value_Output *vo = new Fl_Value_Output(400, 10+35*i, 100, 25);
        valueDisplay_[i] = vo;
    }

    w->end();
    w->show();

    Fl::set_idle(idle_func);
    Fl::run();
    delete uscServo_;
    return 0;
}

