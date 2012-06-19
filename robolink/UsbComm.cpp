
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>

#include <boost/lexical_cast.hpp>
#include <stdexcept>

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

void UsbComm::setRTimestamp(double ts)
{
    //  do nothing
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

void UsbComm::message(unsigned char row, unsigned char col, std::string const &msg)
{
    if (col >= 40) {
        throw std::runtime_error(std::string("Bad column in UsbComm::message(): ")
            + boost::lexical_cast<std::string>(col));
        return;
    }
    if (row >= 4) {
        throw std::runtime_error(std::string("Bad row in UsbComm::message(): ")
            + boost::lexical_cast<std::string>(row));
        return;
    }
    size_t s = msg.size();
    if (s > 40 - col) {
        s = 40 - col;
    }
    char const *str = msg.c_str();
    while (s > 0) {
        unsigned char n = (unsigned char)(s & 0xff);
        if (n > 14) {
            n = 14;
        }
        char msg[19];
        msg[0] = 'W';
        msg[1] = 0x11;  //  display node
        msg[2] = n + 2; //  row, col, data
        msg[3] = row;
        msg[4] = col;
        memcpy(&msg[5], str, n);
        str += n;
        col += n;
        s -= n;
        int i = write(fd_, msg, 5 + n);
        if (i < 0) {
            perror("UsbComm::message()");
            break;
        }
    }
}

