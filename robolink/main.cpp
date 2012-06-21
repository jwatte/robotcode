
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
#include <FL/Fl_Output.H>

#include "UsbComm.h"
#include "Commands.h"
#include "Board.h"
#include "Parser.h"
#include "BoardTile.h"
#include "Voltage.h"
#include "AsyncVideoCapture.h"
#include "ImageDisplay.h"
#include "Decisions.h"
#include "args.h"
#include "DecisionPanel.h"





MotorPowerBoard &motorPower = *new MotorPowerBoard();
Board &estop = *new Board(bidEstop);
SensorBoard &sensors = *new SensorBoard();
UsbLinkBoard &usbLink = *new UsbLinkBoard();
IMUBoard &imu = *new IMUBoard();

UsbComm *g_usb;

Parser p;
Voltage voltage;
DecisionPanel decisionPanel;
Fl_Box *fps_;



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

Talker postCapture_;
int captureN_;
double captureStart_;

//  My God! This looks like polling!
//  The reason it's like this, is that FLTK doesn't provide
//  a separate "wake up the main thread" function. I could 
//  perhaps build one with a FIFO inside AsyncVideoCapture, 
//  but this seems good enough. The good news here is that 
//  the USB camera will never run ahead of processing by more 
//  than a frame, so if I'm processing at 10 Hz, I still won't 
//  have more than a frame's worth of latency.
void cap_callback(void *ac)
{
    AsyncVideoCapture *acap = (AsyncVideoCapture *)ac;
    if (acap->gotFrame()) {
        double t = now();
        if (captureN_ == 30 || (t - captureStart_ > 4.9)) {
            float fps = captureN_ / (t - captureStart_);
            captureN_ = 0;
            static char buf[20];
            sprintf(buf, "%.1f", fps);
            fps_->label(buf);
            std::cerr << buf << " fps" << std::endl;
        }
        if (captureN_ == 0) {
            captureStart_ = t;
        }
        ++captureN_;
        postCapture_.invalidate();
    }
    Fl::add_timeout(0.005, &cap_callback, ac);
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

class ImageInvalidate : public Listener
{
public:
    ImageInvalidate(Talker *sig, AsyncVideoCapture *avc, ImageDisplay *left, ImageDisplay *right) :
        sig_(sig),
        avc_(avc),
        left_(left),
        right_(right)
    {
        sig_->add_listener(this);
    }
    ~ImageInvalidate()
    {
        sig_->remove_listener(this);
    }
    void invalidate()
    {
        VideoFrame *vf = avc_->next();
        left_->invalidate(vf, VideoFrame::IndexLeft);
        right_->invalidate(vf, VideoFrame::IndexRight);
    }

    Talker *sig_;
    AsyncVideoCapture *avc_;
    ImageDisplay *left_;
    ImageDisplay *right_;
};


void make_window(AsyncVideoCapture *avc, Talker *postCapture)
{
    Fl::visual(FL_RGB|FL_DOUBLE);
    fl_register_images();
    Fl_Window *win = new Fl_Window(1280, 720);
    fps_ = new Fl_Box(0, 690, 60, 30);
    Fl_Pack *pack = new Fl_Pack(5, 20, 1270, 200);
    pack->type(Fl_Pack::HORIZONTAL);
    pack->spacing(5);
    (new MotorPowerBoardTile(&motorPower))->make_widgets();
    (new BoardTile(&estop))->make_widgets();
    (new BoardTile(&sensors))->make_widgets();
    (new BoardTile(&usbLink))->make_widgets();
    (new BoardTile(&imu))->make_widgets();
    pack->end();
    pack = new Fl_Pack(2, 225, 1270, 200);
    pack->type(Fl_Pack::HORIZONTAL);
    pack->spacing(5);
    ImageDisplay *id0 = new ImageDisplay();
    id0->box_->size(1280/4, 720/4);
    ImageDisplay *id1 = new ImageDisplay();
    id1->box_->size(1280/4, 720/4);
    voltage.init(&motorPower, &usbLink);
    decisionPanel.init();
    pack->end();
    win->end();
    win->show();
    ImageInvalidate *ii = new ImageInvalidate(postCapture, avc, id0, id1);
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
    AsyncVideoCapture *avc = new AsyncVideoCapture(camL, camR);
    if (!avc->open())
    {
        std::cerr << "Can't open video capture: " << camL << ", " << camR << std::endl;
        return 1;
    }
    make_window(avc, &postCapture_);

    //  Set up processing pump
    Fl::add_fd(g_usb->fd_, on_uc, &ucinfo);
    Fl::add_timeout(0.025, &cap_callback, avc);
    //Fl::add_timeout(0.25, &time_callback, 0);
    Decisions *d = new Decisions(
        &motorPower, &estop, &sensors, &usbLink, avc, &postCapture_, &decisionPanel);
    return Fl::run();
}



