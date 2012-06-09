
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>

#include "UsbComm.h"


UsbComm::UsbComm(std::string const &name)
{
    strncpy(name_, name.c_str(), sizeof(name_));
    name_[sizeof(name_)-1] = 0;
    fd_ = -1;
}

UsbComm::~UsbComm()
{
    close();
}

bool UsbComm::open()
{
    if (fd_ < 0)
    {
        fd_ = ::open(name_, O_RDWR | O_NONBLOCK);
        if (fd_ < 0)
        {
            perror("open()");
            return false;
        }
        setup();
    }
    return true;
}

void UsbComm::close()
{
    if (fd_ >= 0)
    {
        ::close(fd_);
        fd_ = -1;
    }
}

int UsbComm::read1()
{
    if (fd_ < 0)
    {
        return -1;
    }
    char ch = 0;
    if (read(fd_, &ch, 1) != 1)
    {
        return -1;
    }
    return (unsigned char)ch;
}

void UsbComm::setup()
{
    termios tios;
    tcgetattr(fd_, &tios);
    tios.c_iflag = BRKINT | IGNPAR;
    tios.c_oflag = 0;
    tios.c_cflag = CS8 | CREAD | CLOCAL;
    tios.c_lflag = 0;
    cfmakeraw(&tios);
    cfsetispeed(&tios, B115200);
    cfsetospeed(&tios, B115200);
    tcsetattr(fd_, TCSANOW, &tios);
}

