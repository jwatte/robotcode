
#include <stdio.h>
#include <USBLink.h>
#include <Settings.h>
#include <util.h>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>


#define RET_CURRENT_VALUES (0x40 | 0x08 | 0x02) //  ret 4, RET, sizeix 2
#define RET_COUNTER_VALUES (0x50 | 0x08 | 0x05) //  ret 5, RET, sizeix 5

boost::shared_ptr<Module> usb_;
boost::shared_ptr<boost::thread> usbThread_;
unsigned char lastSeq_;
unsigned char nextSeq_;


unsigned char currents[2];
unsigned short counters[4];

void usb_thread_fn() {
    sched_param parm = { .sched_priority = 25 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "USBLink::thread_fn(): pthread_setschedparam(): " << err << std::endl;
    }

    while (true) {

        usb_->step();

        while (true) {
            //  drain receive queue
            size_t sz = 0;
            USBLink *link = usb_->cast_as<USBLink>();
            unsigned char const *ptr = link->begin_receive(sz);
            if (!ptr) {
                link->end_receive(0);
                break;
            }
            unsigned char const *end = ptr + sz;
            lastSeq_ = *ptr;
            ++ptr;
            while (ptr < end) {
                switch (*ptr) {
                    case RET_CURRENT_VALUES:
                        memcpy(currents, ptr+1, 2);
                        ptr += 3;
                        break;
                    case RET_COUNTER_VALUES:
                        memcpy(counters, ptr+1, 8);
                        ptr += 9;
                        break;
                    default:
                        link->end_receive(sz);
                        throw std::runtime_error("unknown command received from USB: "
                            + hexnum(*ptr));
                }
            }
            link->end_receive(sz);
        }
        usleep(3000);
    }
}


int main() {
    boost::shared_ptr<Settings> set(Settings::load("robot.json"));
    usb_ = USBLink::open(set, boost::shared_ptr<Logger>());
    usbThread_ = boost::shared_ptr<boost::thread>(new boost::thread, boost::bind(usb_thread_fn));

    while (true) {
        std::cerr << counters[0] << " " << counters[1] << " " << counters[2] << " " << counters[3] << std::endl;
        usleep(1000000);
    }
}

