
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>
#include <sys/fcntl.h>
#include <termios.h>

#include <string>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <fstream>

#include "../robolink/args.h"

std::string usb("/dev/ttyACM2");

bool set_option(std::string const &key, std::string const &value)
{
    if (key == "usb")
    {
        usb = value;
    }
    else
    {
        std::cerr << "unknown option: " << key << std::endl;
        return false;
    }
    return true;
}

int main(int argc, char const *argv[])
{
    --argc;
    ++argv;
    parse_args(argc, argv, set_option);
    int fd = open(usb.c_str(), O_RDWR);
    if (fd < 0)
    {
        perror(usb.c_str());
        return 1;
    }
    termios tio;
    tcgetattr(fd, &tio);
    cfmakeraw(&tio);
    cfsetspeed(&tio, B115200);
    tcsetattr(fd, TCSANOW, &tio);
    while (true)
    {
        unsigned char ch;
        int r = read(fd, (char *)&ch, 1);
        if (r < 1)
        {
            break;
        }
        char buf[10];
        if (ch == 0xed) //  sync byte
        {
            std::cout << std::endl;
        }
        sprintf(buf, "%02x (%c) ", ch, (ch < 32 || ch > 126) ? '?' : ch);
        std::cout << buf << std::flush;
    }
    return 0;
}
