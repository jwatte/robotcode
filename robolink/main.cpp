
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <termios.h>
#include <string.h>

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





MotorPowerBoard &motorPower = *new MotorPowerBoard();
Board &estop = *new Board(bidEstop);
Board &sensors = *new Board(bidSensors);
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

void cap_callback(void *)
{
    vcap0->step();
    vcap1->step();
    Fl::add_timeout(0.025, &cap_callback, 0);
}

int main(int argc, char const *argv[])
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
    UsbComm uc("/dev/ttyACM2");
    if (!uc.open())
    {
        return 1;
    }
    vcap0 = new VideoCapture("/dev/video0");
    id0->set_source(vcap0);
    vcap1 = new VideoCapture("/dev/video1");
    id1->set_source(vcap1);
    Fl::add_fd(uc.fd_, on_uc, &uc);
    Fl::add_timeout(0.025, &cap_callback, 0);
    return Fl::run();
}



