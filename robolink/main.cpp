
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

#include "UsbComm.h"
#include "Commands.h"
#include "Board.h"
#include "Parser.h"





Board motorPower(bidMotorPower);
Board estop(bidEstop);
Board sensors(bidSensors);
Board usbLink(bidUsbLink);

Parser p;

int main(int argc, char const *argv[])
{
    Fl_Window *win = new Fl_Window(1280, 720);
    win->end();
    win->show();
    UsbComm uc("/dev/ttyACM0");
    if (!uc.open())
    {
        return 1;
    }
    return Fl::run();
}



