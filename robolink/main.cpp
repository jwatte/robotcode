
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>

#include <string>
#include <iostream>
#include <fstream>

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Pack.H>
#include <FL/Fl_Shared_Image.H>

#include "UsbComm.h"
#include "Commands.h"
#include "Board.h"
#include "Parser.h"
#include "BoardTile.h"
#include "Voltage.h"
#include "VideoCapture.h"
#include "ImageDisplay.h"
#include "Decisions.h"
#include "CaptureFile.h"
#include "args.h"





MotorPowerBoard &motorPower = *new MotorPowerBoard();
Board &estop = *new Board(bidEstop);
SensorBoard &sensors = *new SensorBoard();
UsbLinkBoard &usbLink = *new UsbLinkBoard();
IMUBoard &imu = *new IMUBoard();

UsbComm *g_usb;

Parser p;
Voltage voltage;



class InitNow
{
public:
    InitNow() : time_at_start(0) {
        time_at_start = now();
    }
    double time_at_start;
    double now() {
        struct timeval tv = { 0 };
        gettimeofday(&tv, 0);
        double t = (double)tv.tv_sec + (double)tv.tv_usec * 0.000001;
        return t - time_at_start;
    }
};

double now()
{
    static InitNow theNow;
    return theNow.now();
}


struct UC_Info {
    IReader *rd;
    IWriter *wr;
};
static UC_Info ucinfo;

void on_uc(int fd, void *ucp)
{
    UC_Info *uci = (UC_Info *)ucp;
    IReader *ir = uci->rd;
    IWriter *wr = uci->wr;
    double n = now();
    ir->setRTimestamp(n);
    wr->setWTimestamp(n);
    int ch = 0;
    while ((ch = ir->read1()) >= 0)
    {
        wr->write1(ch);
        p.on_char(ch);
    }
}

VideoCapture *vcapL;
VideoCapture *vcapR;

Talker postCapture_;

void cap_callback(void *)
{
    vcapL->step();
    vcapR->step();
    Fl::add_timeout(0.025, &cap_callback, 0);
    postCapture_.invalidate();
}

void time_callback(void *)
{
    char buf[30];
    time_t t;
    struct tm tmm;
    time(&t);
    strftime(buf, 30, "%H:%M:%S", localtime_r(&t, &tmm));
    fprintf(stderr, "time=%s\n", buf);
    g_usb->message(0, 0, buf);
    Fl::add_timeout(0.25, &time_callback, 0);
}

void make_window(VideoCapture *vcapL, VideoCapture *vcapR)
{
    Fl::visual(FL_RGB|FL_DOUBLE);
    fl_register_images();
    Fl_Window *win = new Fl_Window(1280, 720);
    Fl_Pack *pack = new Fl_Pack(5, 20, 1270, 200);
    pack->type(Fl_Pack::HORIZONTAL);
    pack->spacing(5);
    (new MotorPowerBoardTile(&motorPower))->make_widgets();
    (new BoardTile(&estop))->make_widgets();
    (new BoardTile(&sensors))->make_widgets();
    (new BoardTile(&usbLink))->make_widgets();
    (new BoardTile(&imu))->make_widgets();
    voltage.init(&motorPower, &usbLink);
    pack->end();
    ImageDisplay *id0 = new ImageDisplay();
    id0->box_->resize(10, 232, 1280/4, 720/4);
    ImageDisplay *id1 = new ImageDisplay();
    id1->box_->resize(340, 232, 1280/4, 720/4);
    win->end();
    win->show();
    id0->set_source(vcapL);
    id1->set_source(vcapR);
}

std::string camL("/dev/video0");
std::string camR("/dev/video1");
std::string usb("/dev/ttyACM2");
std::string capture("capture.bin");
std::string playback("");

struct opt {
    char const *name;
    std::string *value;
}
options[] = {
    { "camL", &camL },
    { "camR", &camR },
    { "usb", &usb },
    { "capture", &capture },
    { "playback", &playback },
};
bool set_option(std::string const &key, std::string const &value)
{
    for (size_t i = 0; i != sizeof(options)/sizeof(options[0]); ++i)
    {
        if (key == options[i].name)
        {
            *options[i].value = value;
            return true;
        }
    }
    return false;
}

class DummyWr : public IWriter {
public:
    virtual void setWTimestamp(double) {}
    virtual void write1(int) {}
};
DummyWr g_dummyWr;

int main(int argc, char const **argv)
{
    --argc;
    ++argv;
    if (!parse_args(argc, argv, set_option))
    {
        std::cerr << "Errors parsing arguments." << std::endl;
        exit(1);
    }
    g_usb = new UsbComm(usb);
    if (!g_usb->open())
    {
        std::cerr << "Can't open USB: " << usb << std::endl;
        return 1;
    }
    ucinfo.rd = g_usb;
    ucinfo.wr = &g_dummyWr;
    vcapL = new VideoCapture(camL);
    vcapR = new VideoCapture(camR);
    make_window(vcapL, vcapR);

    //  playback?
    CaptureFile *pb = 0;
    if (playback.size())
    {
        std::cerr << "Playback from file: " << playback << std::endl;
        if (playback == capture)
        {
            std::cerr << "Can't playback the same file that's captured." << std::endl;
            return 1;
        }
        pb = new CaptureFile(playback, false);
        if (!pb->open())
        {
            std::cerr << "Can't open playback file: " << playback << std::endl;
            return 1;
        }
        ucinfo.rd = pb;
    }

    //  capture?
    CaptureFile *ca = 0;
    if (capture.size())
    {
        std::cerr << "Capture to file: " << capture << std::endl;
        ca = new CaptureFile(capture, true);
        if (!ca->open())
        {
            std::cerr << "Can't open capture file: " << capture << std::endl;
            return 1;
        }
        ucinfo.wr = ca;
    }

    //  Set up processing pump
    Fl::add_fd(pb ? pb->fd_ : g_usb->fd_, on_uc, &ucinfo);
    Fl::add_timeout(0.025, &cap_callback, 0);
    //Fl::add_timeout(0.25, &time_callback, 0);
    Decisions *d = new Decisions(
        &motorPower, &estop, &sensors, &usbLink, vcapL, vcapR, &postCapture_);
    return Fl::run();
}



