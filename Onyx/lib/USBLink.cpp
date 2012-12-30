
#include "USBLink.h"
#include "Settings.h"
#include "PropertyImpl.h"
#include <libusb.h>
#include <boost/bind.hpp>
#include <stdexcept>
#include <sstream>
#include <assert.h>

#include "protocol.h"


#define WRITE_USB 1


class Transfer {
public:
    Transfer(libusb_device_handle *dh, unsigned char iep, unsigned char oep) :
        dh_(dh),
        iep_(iep),
        oep_(oep),
        inPack_(0),
        inReady_(0),
        inXfer_(libusb_alloc_transfer(0)),
        outPack_(0),
        outXfer_(libusb_alloc_transfer(0)) {

        boost::unique_lock<boost::mutex> lock(lock_);
        start_in_inner();
    }
    ~Transfer() {
        dh_ = 0;
        libusb_free_transfer(inXfer_);
        libusb_free_transfer(outXfer_);
        BOOST_FOREACH(auto p, inQueue_) {
            p->destroy();
        }
        BOOST_FOREACH(auto p, outQueue_) {
            p->destroy();
        }
    }

    unsigned int in_ready() {
        return inReady_.nonblocking_available();
    }
    Packet *in_dequeue() {
        inReady_.acquire();
        boost::unique_lock<boost::mutex> lock(lock_);
        Packet *ret = inQueue_.front();
        inQueue_.pop_front();
        return ret;
    }

    void out_write(void const *data, size_t sz) {
        Packet *p = Packet::create();
        p->set_size(sz);
        memcpy(p->buffer(), data, sz);
        boost::unique_lock<boost::mutex> lock(lock_);
        outQueue_.push_back(p);
        start_out_inner();
    }

private:
    //  must be called with lock held
    void start_out_inner() {
        #if WRITE_USB
        if (!outPack_ && !outQueue_.empty()) {
            outPack_ = outQueue_.front();
            outQueue_.pop_front();
            memset(outXfer_, 0, sizeof(*outXfer_));
            outXfer_->dev_handle = dh_;
            libusb_fill_bulk_transfer(outXfer_, dh_, oep_, outPack_->buffer(), outPack_->size(),
                    &Transfer::out_callback, this, 1000); //  a second is a long time!
            int err = libusb_submit_transfer(outXfer_);
            if (err != 0) {
                std::cerr << "libusb_submit_transfer() failed writing to board: "
                    << libusb_error_name(err) << " (" << err << ")" << std::endl;
                outPack_->destroy();
                outPack_ = 0;
            }
        }
        #endif
    }

    static void out_callback(libusb_transfer *cbArg) {
        #if WRITE_USB
        reinterpret_cast<Transfer *>(cbArg->user_data)->out_complete();
        #endif
    }

    void out_complete() {
        #if WRITE_USB
        boost::unique_lock<boost::mutex> lock(lock_);
        outPack_->destroy();
        outPack_ = 0;
        start_out_inner();
        #endif
    }

    //  must be called with lock held
    void start_in_inner() {
        if (!inPack_) {
            inPack_ = Packet::create();
            memset(inXfer_, 0, sizeof(*inXfer_));
            inXfer_->dev_handle = dh_;
            libusb_fill_bulk_transfer(inXfer_, dh_, iep_, inPack_->buffer(), inPack_->max_size(),
                    &Transfer::in_callback, this, 1000); //  a second is a long time!
            int err = libusb_submit_transfer(inXfer_);
            if (err != 0) {
                std::cerr << "libusb_submit_transfer() failed reading from board: "
                    << libusb_error_name(err) << " (" << err << ")" << std::endl;
                inPack_->destroy();
                inPack_ = 0;
            }
        }
    }

    static void in_callback(libusb_transfer *cbArg) {
        reinterpret_cast<Transfer *>(cbArg->user_data)->in_complete();
    }

    void in_complete() {
        if (inXfer_->status != LIBUSB_TRANSFER_COMPLETED) {
            std::cerr << "transfer status: " << inXfer_->status << std::endl;
        }
        boost::unique_lock<boost::mutex> lock(lock_);
        inPack_->set_size(inXfer_->actual_length);
        inQueue_.push_back(const_cast<Packet *>(inPack_));
        inPack_ = 0;
        inReady_.release();
        start_in_inner();
    }

    libusb_device_handle *dh_;
    unsigned char iep_;
    unsigned char oep_;
    boost::mutex lock_;

    Packet *volatile inPack_;
    std::deque<Packet *> inQueue_;
    semaphore inReady_;
    libusb_transfer *inXfer_;

    Packet *volatile outPack_;
    std::deque<Packet *> outQueue_;
    libusb_transfer *outXfer_;
};


boost::shared_ptr<Module> USBLink::open(boost::shared_ptr<Settings> const &set) {
    std::string vid("f000");
    std::string pid("0002");
    std::string ep_input("82");
    std::string ep_output("3");
    auto v = set->get_value("vid");
    if (!!v) {
        vid = v->get_string();
    }
    v = set->get_value("pid");
    if (!!v) {
        pid = v->get_string();
    }
    v = set->get_value("input");
    if (!!v) {
        ep_input = v->get_string();
    }
    v = set->get_value("output");
    if (!!v) {
        ep_output = v->get_string();
    }
    return boost::shared_ptr<Module>(new USBLink(vid, pid, ep_input, ep_output));
}

void USBLink::step() {
    while (xfer_->in_ready()) {
        Packet *pack = xfer_->in_dequeue();
        ++inPackets_;
        size_t size = pack->size();
        if (size > 0) {
            bool drop = false;
            if (sendBufEnd_ + size > sizeof(sendBuf_)) {
                if (sendBufEnd_ + size - sendBufBegin_ > sizeof(sendBuf_)) {
                    ++dropPackets_;
                    //  drop it
                    drop = true;
                }
                else {
                    memmove(sendBuf_, &sendBuf_[sendBufBegin_], sendBufEnd_-sendBufBegin_);
                    sendBufEnd_ -= sendBufBegin_;
                    sendBufBegin_ = 0;
                }
            }
            if (!drop) {
                unsigned char const *pp = (unsigned char const *)pack->buffer();
                memcpy(&sendBuf_[sendBufEnd_], pp, size);
                sendBufEnd_ += size;
                assert(sendBufEnd_ <= sizeof(sendBuf_));
            }
        }
    }
    inPacketsProperty_->set<long>(inPackets_);
    outPacketsProperty_->set<long>(outPackets_);
    dropPacketsProperty_->set<long>(dropPackets_);
}

void USBLink::thread_fn() {
    sched_param parm = { .sched_priority = 20 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "Camera::process(): pthread_setschedparam(): " << err << std::endl;
    }

    while (!thread_->interruption_requested()) {
        struct timeval tv = { 0, 10000 };
        libusb_handle_events_timeout_completed(ctx_, &tv, 0);
    }
    std::cerr << "USBLink::thread_fn() returning" << std::endl;
}

std::string const &USBLink::name() {
    return name_;
}

size_t USBLink::num_properties() {
    return 3;
}

boost::shared_ptr<Property> USBLink::get_property_at(size_t ix) {
    switch (ix) {
    case 0: return inPacketsProperty_;
    case 1: return outPacketsProperty_;
    case 2: return dropPacketsProperty_;
    default:
        throw std::runtime_error("index out of range in USBLink::get_property_at()");
    }
}

USBLink::~USBLink() {
    thread_->interrupt();
    pickup_.release();
    return_.release();
    thread_->join();
    if (dh_) {
        libusb_close(dh_);
    }
    if (ctx_ ) {
        libusb_exit(ctx_);
    }
    delete xfer_;
}

static std::string str_in_packets("in_packets");
static std::string str_out_packets("out_packets");
static std::string str_drop_packets("drop_packets");

USBLink::USBLink(std::string const &vid, std::string const &pid, std::string const &ep_input, std::string const &ep_output) :
    vid_(vid),
    pid_(pid),
    ep_input_(ep_input),
    ep_output_(ep_output),
    ctx_(0),
    dh_(0),
    xfer_(0),
    pickup_(0),
    return_(0),
    inPackets_(0),
    outPackets_(0),
    dropPackets_(0),
    inPacketsProperty_(new PropertyImpl<long>(str_in_packets)),
    outPacketsProperty_(new PropertyImpl<long>(str_out_packets)),
    dropPacketsProperty_(new PropertyImpl<long>(str_drop_packets)),
    sendBufBegin_(0),
    sendBufEnd_(0),
    name_(vid + ":" + pid) {

    if (libusb_init(&ctx_) < 0) {
        throw std::runtime_error("Error opening libusb for " + name_);
    }
    char *o = 0;
    ivid_ = (unsigned short)strtol(vid_.c_str(), &o, 16);
    ipid_ = (unsigned short)strtol(pid_.c_str(), &o, 16);
    iep_ = (unsigned short)strtol(ep_input_.c_str(), &o, 16);
    oep_ = (unsigned short)strtol(ep_output_.c_str(), &o, 16);
    dh_ = libusb_open_device_with_vid_pid(ctx_, ivid_, ipid_);
    if (!dh_ ) {
        throw std::runtime_error("Could not find USB device " + name_);
    }
    int er;
    er = libusb_set_configuration(dh_, 1);
    if (er != 0) {
        throw std::runtime_error("warning: Could not set configuration on comm board " +
            name_ + ": " + libusb_error_name(er));
    }
    er = libusb_claim_interface(dh_, 0);
    if (er != 0) {
        throw std::runtime_error("Could not claim USB interface for comm board " +
            name_ + ". Is another process using it? " + libusb_error_name(er));
    }
    xfer_ = new Transfer(dh_, iep_, oep_);
    libusb_device_descriptor ldd;
    er = libusb_get_device_descriptor(libusb_get_device(dh_), &ldd);
    if (er < 0) {
        throw std::runtime_error("Could not find USB descriptor for comm board " + 
            name_);
    }
    thread_ = boost::shared_ptr<boost::thread>(new boost::thread(
        boost::bind(&USBLink::thread_fn, this)));
}

void USBLink::raw_send(void const *data, unsigned char sz) {
    if (sz > 64) {
        throw std::runtime_error("Too large buffer in raw_send()");
    }
    xfer_->out_write(data, sz);
    ++outPackets_;
}

unsigned char const *USBLink::begin_receive(size_t &oSize) {
    oSize = sendBufEnd_ - sendBufBegin_;
    return &sendBuf_[sendBufBegin_];
}

void USBLink::end_receive(size_t size) {
    assert(size <= sendBufEnd_ - sendBufBegin_);
    sendBufBegin_ += size;
    if (sendBufBegin_ == sendBufEnd_) {
        sendBufBegin_ = sendBufEnd_ = 0;
    }
}

