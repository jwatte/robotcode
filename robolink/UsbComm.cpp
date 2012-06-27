
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <stdio.h>
#include <unistd.h>

#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>
#include <boost/bind.hpp>
#include <stdexcept>

#include "UsbComm.h"


#define BAUD_RATE B38400

UsbComm::UsbComm(std::string const &name) :
    head_(0),
    tail_(0),
    thread_(0)
{
    strncpy(name_, name.c_str(), sizeof(name_));
    name_[sizeof(name_)-1] = 0;
    fd_ = -1;
    stalled_ = false;
}

UsbComm::~UsbComm()
{
    close();
}

bool UsbComm::open()
{
    if (fd_ < 0)
    {
        fd_ = ::open(name_, O_RDWR);
        if (fd_ < 0)
        {
            perror("open()");
            return false;
        }
        setup();
        thread_ = new boost::thread(boost::bind(&UsbComm::read_thread, this));
    }
    return true;
}

void UsbComm::read_thread()
{
    int wfd = ::open("serial.bin", O_RDWR | O_CREAT | O_TRUNC, 0664);
    while (fd_ >= 0) {
        int diff = PIPE_SIZE - (int)(head_ - tail_);
        if (diff > 0) {
            unsigned char volatile *rpos = &pipe_[head_ & -PIPE_SIZE];
            int r = sizeof(pipe_) - (head_ & -PIPE_SIZE);
            if (r == PIPE_SIZE) {
                r = diff;
                rpos = &pipe_[0];
            }
            if (r > diff) {
                r = diff;
            }
            if (r > 8) {
                r = 8;
            }
            int n = ::read(fd_, const_cast<unsigned char *>(rpos), r);
            if (n > 0) {
                std::cerr << "read(" << fd_ << ", " << r << ") = " << n << std::endl;
                ::write(wfd, const_cast<unsigned char *>(rpos), n);
            }
            if (n > 0) {
                head_ += n;
            }
            else {
                int en = errno;
                if (n == 0 || en == EWOULDBLOCK || en == EAGAIN) {
                    ::usleep(1000);
                }
                else {
                    perror("UsbComm::read_thread()");
                    break;
                }
            }
        }
    }
    std::cerr << "read_thread(): quitting" << std::endl;
    fsync(wfd);
    ::close(wfd);
}


void UsbComm::close()
{
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
        if (thread_) {
            thread_->join();
            thread_ = 0;
        }
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
    if ((int)(head_ - tail_) > 0) {
        int ret = pipe_[tail_ & -PIPE_SIZE];
        ++tail_;
        return ret;
    }
    return -1;
}

void UsbComm::setup()
{
    termios tios;
    tcgetattr(fd_, &tios);
    tios.c_iflag = IGNBRK | IGNPAR;
    tios.c_oflag = 0;
    tios.c_cflag = CS8 | CREAD;
    tios.c_lflag = 0;
    memset(tios.c_cc, 0, sizeof(tios.c_cc));
    tios.c_cc[VMIN] = 8;
    tios.c_cc[VTIME] = 0;
    cfmakeraw(&tios);
    cfsetispeed(&tios, BAUD_RATE);
    cfsetospeed(&tios, BAUD_RATE);
    tcsetattr(fd_, TCSANOW, &tios);
}

void UsbComm::message(unsigned char row, unsigned char col, std::string const &msg)
{
    return; // todo: remove me
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

void UsbComm::write_reg(unsigned char node, unsigned char reg, unsigned char n, void const *d)
{
    return; //  todo: remove me
    assert(n <= 32);
    char msg[36];
    msg[0] = 'W';
    msg[1] = node;
    msg[2] = n;
    memcpy(&msg[3], d, n);
    char const *w = msg;
    int tw = 3 + n;
    while (tw > 0) {
        int i = write(fd_, w, tw);
        if (i < 0) {
            int en = errno;
            if (en != EWOULDBLOCK && en != EAGAIN) {
                perror("UsbComm::write_reg()");
                throw new std::runtime_error(std::string("Could not write to UsbComm: ") + strerror(en));
            }
            else if (!stalled_) {
                stalled_ = true;
                std::cerr << "Stalling UsbComm write" << std::endl;
                ::usleep(100);
            }
            else {
                return;
            }
        }
        else {
            stalled_ = false;
            tw -= i;
            w += i;
        }
    }
}


