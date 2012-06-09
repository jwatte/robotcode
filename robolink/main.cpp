
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

VideoCapture *vcap0;
VideoCapture *vcap1;

Talker postCapture_;

void cap_callback(void *)
{
    vcap0->step();
    vcap1->step();
    Fl::add_timeout(0.025, &cap_callback, 0);
    postCapture_.invalidate();
}

void make_window(VideoCapture *vcap0, VideoCapture *vcap1)
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
    id0->set_source(vcap0);
    id1->set_source(vcap1);
}

std::string cam0("/dev/video0");
std::string cam1("/dev/video1");
std::string usb("/dev/ttyACM2");

bool set_option(std::string const &key, std::string const &value)
{
    if (key == "cam0")
    {
        cam0 = value;
    }
    else if (key == "cam1")
    {
        cam1 = value;
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

void read_config_file(std::string const &path)
{
    std::ifstream ifile(path.c_str(), std::ifstream::in | std::ifstream::binary);
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
}

int main(int argc, char const **argv)
{
    --argc;
    ++argv;
    parse_args(argc, argv);
    vcap0 = new VideoCapture(cam0);
    vcap1 = new VideoCapture(cam1);
    make_window(vcap0, vcap1);
    UsbComm uc(usb);
    if (!uc.open())
    {
        return 1;
    }
    Fl::add_fd(uc.fd_, on_uc, &uc);
    Fl::add_timeout(0.025, &cap_callback, 0);
    Decisions *d = new Decisions(
        &motorPower, &estop, &sensors, &usbLink, vcap0, vcap1, &uc, &postCapture_);
    return Fl::run();
}



