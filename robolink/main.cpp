
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <string.h>
#include <ctype.h>

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
#include "args.h"





MotorPowerBoard &motorPower = *new MotorPowerBoard();
Board &estop = *new Board(bidEstop);
SensorBoard &sensors = *new SensorBoard();
UsbLinkBoard &usbLink = *new UsbLinkBoard();
IMUBoard &imu = *new IMUBoard();

UsbComm *g_usb;

Parser p;
Voltage voltage;

void on_uc(int fd, void *ucp)
{
    UsbComm *uc = (UsbComm *)ucp;
    int ch = 0;
    while ((ch = uc->read1()) >= 0)
    {
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

bool set_option(std::string const &key, std::string const &value)
{
    if (key == "camL")
    {
        camL = value;
    }
    else if (key == "camR")
    {
        camR = value;
    }
    else if (key == "usb")
    {
        usb = value;
    }
    else
    {
        return false;
    }
    return true;
}

int main(int argc, char const **argv)
{
    --argc;
    ++argv;
    if (!parse_args(argc, argv, set_option)) {
        exit(1);
    }
    vcapL = new VideoCapture(camL);
    vcapR = new VideoCapture(camR);
    make_window(vcapL, vcapR);
    g_usb = new UsbComm(usb);
    if (!g_usb->open())
    {
        return 1;
    }
    Fl::add_fd(g_usb->fd_, on_uc, g_usb);
    Fl::add_timeout(0.025, &cap_callback, 0);
    //Fl::add_timeout(0.25, &time_callback, 0);
    Decisions *d = new Decisions(
        &motorPower, &estop, &sensors, &usbLink, vcapL, vcapR, g_usb, &postCapture_);
    return Fl::run();
}



