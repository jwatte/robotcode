
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





MotorPowerBoard &motorPower = *new MotorPowerBoard();
Board &estop = *new Board(bidEstop);
SensorBoard &sensors = *new SensorBoard();
UsbLinkBoard &usbLink = *new UsbLinkBoard();
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

static void trim(std::string &s)
{
    size_t front = 0;
    size_t back = s.size();
    while (front < back && isspace(s[front]))
    {
        ++front;
    }
    while (back > front && isspace(s[back-1]))
    {
        --back;
    }
    s = s.substr(front, back-front);
}

bool readCfgFile_ = false;

void read_config_file(std::string const &path)
{
    readCfgFile_ = true;
    std::ifstream ifile(path.c_str(), std::ifstream::in | std::ifstream::binary);
    if (!ifile)
    {
        std::cerr << path << ": file not readable" << std::endl;
        exit(1);
    }
    char buf[1024];
    int lineno = 0;
    while (!ifile.eof())
    {
        buf[0] = 0;
        ifile.getline(buf, 1024);
        ++lineno;
        size_t off = 0;
        while (isspace(buf[off]))
        {
            ++off;
        }
        if (buf[0] && buf[0] != '#')
        {
            std::string s(&buf[off]);
            size_t off = s.find('=');
            if (off == std::string::npos)
            {
                std::cerr << path << ":" << lineno << ": missing value for: " << s << std::endl;
                exit(1);
            }
            std::string v(s.substr(off+1));
            s = s.substr(0, off);
            trim(s);
            trim(v);
            if (!set_option(s, v))
            {
                std::cerr << path << ":" << lineno << ": unknown option: " << s << std::endl;
                exit(1);
            }
        }
    }
}

void parse_args(int &argc, char const ** &argv)
{
    while (argc > 0)
    {
        if ((*argv)[0] == '-')
        {
            std::string s;
            if ((*argv)[1] == '-')
            {
                s = (*argv) + 2;
            }
            else
            {
                s = (*argv) + 1;
            }
            std::string v;
            if (s.find('=') == std::string::npos)
            {
                if (argc <= 1)
                {
                    std::cerr << "missing value for: " << s << std::endl;
                    exit(1);
                }
                --argc;
                ++argv;
                v = *argv;
            }
            else
            {
                v = s.substr(s.find('=') + 1);
                s = s.substr(0, s.find('='));
            }
            if (!set_option(s, v))
            {
                std::cerr << "unknown option: " << s << std::endl;
                exit(1);
            }
        }
        else
        {
            read_config_file(*argv);
        }
        --argc;
        ++argv;
    }
    if (!readCfgFile_)
    {
        std::string s(getenv("HOME"));
        s += "/.config/robolink.cfg";
        read_config_file(s.c_str());
    }
}

int main(int argc, char const **argv)
{
    --argc;
    ++argv;
    parse_args(argc, argv);
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



