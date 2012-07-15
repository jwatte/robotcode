
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

#include "Signal.h"
#include "UsbComm.h"
#include <libusb.h>


#define BAUD_RATE B115200

UsbComm::UsbComm(std::string const &devName, IPacketDestination *dst) :
  name_(devName),
  dest_(dst),
  running_(false),
  thread_(0),
  ctx_(0),
  dh_(0)
{
}

UsbComm::~UsbComm()
{
  close();
}

bool UsbComm::open()
{
  if (dh_ != 0) {
    close();
  }
  if (libusb_init(&ctx_) < 0) {
    throw std::runtime_error("Error opening libusb");
  }
  //    todo: parse vid/pid out of name_
  dh_ = libusb_open_device_with_vid_pid(ctx_, 0xf000, 0x0001);
  if (dh_ == 0) {
    throw std::runtime_error("Could not find USB device 0xf000/0x0001 for comm board.");
  }
  libusb_device_descriptor ldd;
  int er;
  er = libusb_get_device_descriptor(libusb_get_device(dh_), &ldd);
  if (er < 0) {
    throw std::runtime_error("Could not find USB descriptor for comm board.");
  }
  er = libusb_set_configuration(dh_, 1);
  if (er < 0) {
    throw std::runtime_error("Could not set configuration on comm board.");
  }
  er = libusb_claim_interface(dh_, 0);
  if (er < 0) {
    throw std::runtime_error("Could not claim USB interface for comm board. Is another process using it?");
  }
  running_ = true;
  thread_ = new boost::thread(boost::bind(&UsbComm::read_func, this));
  return true;
}

void UsbComm::close()
{
  running_ = false;
  if (thread_) {
    thread_->join();
    delete thread_;
    thread_ = 0;
  }
  if (dh_) {
    libusb_close(dh_);
    dh_ = 0;
  }
  if (ctx_) {
    libusb_exit(ctx_);
    ctx_ = 0;
  }
}

void UsbComm::transmit()
{
    boost::unique_lock<boost::mutex> lock(lock_);
    while (Packet *p = received_.dequeue()) {
        dest_->on_packet(p);
    }
}

/* the user wants to send something to a board */
void UsbComm::on_packet(Packet *p)
{
  p->destroy();
}

#define RECV_ENDPOINT 0x82

class Transfer {
public:
    Transfer() : xfer_(0), pack_(0) {
        xfer_ = libusb_alloc_transfer(0);
        busy_ = false;
    }
    ~Transfer() {
        libusb_free_transfer(xfer_);
        if (pack_) {
            pack_->destroy();
        }
    }
    void recv(libusb_device_handle *dh) {
        pack_ = Packet::create();
        busy_ = true;
        xfer_->dev_handle = dh;
        libusb_fill_bulk_transfer(xfer_, dh, RECV_ENDPOINT, pack_->buffer(), pack_->max_size(),
                &Transfer::callback, this, 1000); //  a second is a long time!
        int err = libusb_submit_transfer(xfer_);
        if (err != 0) {
            std::cerr << "libusb_submit_transfer(): " << err << ": " << libusb_error_name(err) << std::endl;
            pack_->destroy();
            pack_ = 0;
            usleep(10000);
            busy_ = false;
        }
    }
    static void callback(libusb_transfer *x) {
        ((Transfer *)x->user_data)->on_callback();
    }
    void on_callback() {
        pack_->set_size(xfer_->actual_length);
        busy_ = false;
    }
    libusb_transfer *xfer_;
    Packet *pack_;
    bool busy_;
};

void UsbComm::read_func()
{
    Transfer xfer;
    while (running_) {
        if (!xfer.busy_) {
            if (xfer.pack_) {
                boost::unique_lock<boost::mutex> lock(lock_);
                if (!received_.enqueue(xfer.pack_)) {
                    std::cerr << "Warning: input queue full; dropping packet." << std::endl;
                    xfer.pack_->destroy();
                }
                xfer.pack_ = 0;
                //  collect a little more data per batch
                usleep(8000);
            }
            xfer.recv(dh_);
        }
        //  Mmm! Polling!
        libusb_handle_events(ctx_);
    }
    std::cerr << "UsbComm::read_func() exiting" << std::endl;
    libusb_cancel_transfer(xfer.xfer_);
}

