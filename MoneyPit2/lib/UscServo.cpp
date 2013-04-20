
#include "UscServo.h"
#include <boost/bind.hpp>
#include <util.h>

#define USC_CMD_SET_TARGET 0x85


UscServo::UscServo(unsigned short vendor, unsigned short id, unsigned short const *centers) :
    ctx_(0),
    dh_(0)
{
    memset(const_cast<short *>(steering_), 0, sizeof(steering_));
    for (int i = 0; i != 4; ++i) {
        centers_[i] = centers[i];
        dirty_[i] = true;
    }
    if (libusb_init(&ctx_) < 0) {
        throw std::runtime_error("Error opening libusb for UscServo");
    }
    dh_ = libusb_open_device_with_vid_pid(ctx_, vendor, id);
    if (!dh_) {
        std::cerr << hexnum(vendor) << ":" << hexnum(id) << " not found" << std::endl;
        throw std::runtime_error("Could not find USB device UscServo");
    }
    int er;
    /*
    er = libusb_set_configuration(dh_, 1);
    if (er != 0) {
        throw std::runtime_error("Could not set configuration on UscServo");
    }
    er = libusb_claim_interface(dh_, 0);
    if (er != 0) {
        throw std::runtime_error("Could not claim USB interface for UscServo");
    }
    */
    libusb_device_descriptor ldd;
    er = libusb_get_device_descriptor(libusb_get_device(dh_), &ldd);
    if (er < 0) {
        throw std::runtime_error("Could not find USB descriptor for UscServo");
    }
    thread_ = boost::shared_ptr<boost::thread>(new boost::thread(boost::bind(&UscServo::usb_thread_fn, this)));
}

UscServo::~UscServo()
{
    thread_->interrupt();
    thread_->join();
    std::cerr << "shitty not shutting down UscServo" << std::endl;
}

void UscServo::steer(unsigned char servo, short delta) {
    //  rely on atomic memory of word-size objects
    steering_[servo] = delta;
    dirty_[servo] = true;
}

void UscServo::usb_thread_fn()
{
    int s = 0;
    while (!thread_->interruption_requested()) {
        struct timeval tv = { 0, 5000 };
        libusb_handle_events_timeout_completed(ctx_, &tv, 0);
        if (dirty_[s]) {
           //  read/clear dirty before steer, to order to avoid races
            dirty_[s] = false;
            unsigned short data = centers_[s] + steering_[s];
            libusb_control_transfer(dh_, 0x40, USC_CMD_SET_TARGET, data, s, 0, 0, 20);
            s += 1;
            if (s == 4) {
                s = 0;
            }
        }
        else {
            usleep(3000);
        }
    }
}


