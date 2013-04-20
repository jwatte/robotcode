
#if !defined(lib_UscServo_h)
#define lib_UscServo_h

#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <libusb.h>

class UscServo {
public:
    UscServo(unsigned short vendor, unsigned short id, unsigned short const *centers);
    ~UscServo();

    void steer(unsigned char servo, short delta);

private:
    volatile short steering_[4];
    volatile unsigned short centers_[4];
    volatile bool dirty_[4];
    libusb_context *ctx_;
    libusb_device_handle *dh_;
    boost::shared_ptr<boost::thread> thread_;

    void usb_thread_fn();
};

#endif  //  lib_UscServo_h
