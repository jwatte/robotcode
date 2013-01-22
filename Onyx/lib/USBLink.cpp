
#include "USBLink.h"
#include "Settings.h"
#include "PropertyImpl.h"
#include "util.h"
#include <libusb.h>
#include <boost/bind.hpp>
#include <stdexcept>
#include <sstream>
#include <assert.h>

//#include "protocol.h"


#define WRITE_USB 1


int inCount_;
int inComplete_;
int outCount_;
int outComplete_;


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
        outXfer_(libusb_alloc_transfer(0)),
        outQueueDepth_(0),
        outRetrying_(false),
        complained_(false) {

        memset(dd_out, 0, sizeof(dd_out));
        dn_out = 0;
        dm_out = 0;
        memset(dd_in, 0, sizeof(dd_in));
        dn_in = 0;
        dm_in = 0;

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
        if (outQueueDepth_ > 16) {
            if (!(outQueueDepth_ & 15)) {
                std::cerr << "outQueueDepth_: " << outQueueDepth_ << std::endl;
            }
            if (outQueueDepth_ >= 100) {
                //  drop the packet
                if (!complained_) {
                    std::cerr << "dropping packet (out queue depth " << outQueueDepth_ << ")" << std::endl;
                    complained_ = false;
                }
                return;
            }
        }
        complained_ = false;
        outQueue_.push_back(p);
        outQueueDepth_ = outQueue_.size();
        //  I think LibUSB is not re-entrant
        // start_out_inner();
    }

    size_t out_queue_depth() {
        return outQueueDepth_;
    }

    void poke() {
        if (!outPack_) {
            boost::unique_lock<boost::mutex> lock(lock_);
            start_out_inner();
        }
    }

    double dd_out[16];
    int dn_out;
    int dm_out;
    double dd_in[16];
    int dn_in;
    int dm_in;

private:
    //  must be called with lock held
    void start_out_inner() {
        #if WRITE_USB
        if (!outPack_ && !outQueue_.empty()) {
            outPack_ = outQueue_.front();
            outQueue_.pop_front();
            outQueueDepth_ = outQueue_.size();
            start_out_xfer();
        }
        #endif
    }

    void start_out_xfer() {
        dd_out[dn_out++ & 0xf] = read_clock();

        memset(outXfer_, 0, sizeof(*outXfer_));
        outXfer_->dev_handle = dh_;
        libusb_fill_bulk_transfer(outXfer_, dh_, oep_, outPack_->buffer(), outPack_->size(),
                &Transfer::out_callback, this, 1000); //  a second is a long time!
        outCount_++;
        int err = libusb_submit_transfer(outXfer_);
        if (err != 0) {
            outCount_--;
            std::cerr << "libusb_submit_transfer() failed writing to board: "
                << libusb_error_name(err) << " (" << err << ")" << std::endl;
            outPack_->destroy();
            outPack_ = 0;
            throw std::runtime_error("Failed writing to board.");
        }
    #if DUMP_WRITE_DATA
        std::cout << std::hex;
        unsigned char *ptr = (unsigned char *)outPack_->buffer();
        for (size_t i = 0, n = outPack_->size(); i != n; ++i) {
            std::cout << " 0x" << (int)ptr[i];
        }
        std::cout << std::dec << std::endl;
    #endif
    }

    static void out_callback(libusb_transfer *cbArg) {
        #if WRITE_USB
        outComplete_++;
        reinterpret_cast<Transfer *>(cbArg->user_data)->out_complete();
        #endif
    }

    void out_complete() {
        #if WRITE_USB
        double t = read_clock();
        t -= dd_out[dm_out++ & 0xf];
        if (t > 0.1) {
            fprintf(stderr, "out %.4f\n", t);
        }

        if (outXfer_->status != LIBUSB_TRANSFER_COMPLETED) {
            std::cerr << "out transfer status: " << outXfer_->status << std::endl;
        }
        boost::unique_lock<boost::mutex> lock(lock_);
        if (outXfer_->status == 1 && !outRetrying_) {
            //re-try
            outRetrying_ = true;
            start_out_xfer();
        }
        else {
            outRetrying_ = false;
            outPack_->destroy();
            outPack_ = 0;
            start_out_inner();
        }
        #endif
    }

    //  must be called with lock held
    void start_in_inner() {
        if (!inPack_) {
            dd_in[dn_in++ & 0xf] = read_clock();
            inPack_ = Packet::create();
            memset(inXfer_, 0, sizeof(*inXfer_));
            inXfer_->dev_handle = dh_;
            libusb_fill_bulk_transfer(inXfer_, dh_, iep_, inPack_->buffer(), inPack_->max_size(),
                    &Transfer::in_callback, this, 1000); //  a second is a long time!
            inCount_++;
            int err = libusb_submit_transfer(inXfer_);
            if (err != 0) {
                inCount_--;
                std::cerr << "libusb_submit_transfer() failed reading from board: "
                    << libusb_error_name(err) << " (" << err << ")" << std::endl;
                inPack_->destroy();
                inPack_ = 0;
            }
        }
    }

    static void in_callback(libusb_transfer *cbArg) {
        inComplete_++;
        reinterpret_cast<Transfer *>(cbArg->user_data)->in_complete();
    }

    void in_complete() {
        double t = read_clock();
        t -= dd_in[dm_in++ & 0xf];
        if (t > 0.1) {
            fprintf(stderr, "in %.4f\n", t);
        }

        if (inXfer_->status != LIBUSB_TRANSFER_COMPLETED) {
            std::cerr << "in transfer status: " << inXfer_->status << std::endl;
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
    size_t outQueueDepth_;
    bool outRetrying_;
    bool complained_;
};


boost::shared_ptr<Module> USBLink::open(boost::shared_ptr<Settings> const &set) {
    std::string vid("f000");
    std::string pid("0002");
    std::string ep_output("2");
    std::string ep_input("81");
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
            recvQ_.push_back(pack);
        }
        else {
            pack->destroy();
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
        std::cerr << "USBLink::thread_fn(): pthread_setschedparam(): " << err << std::endl;
    }

    while (!thread_->interruption_requested()) {
        struct timeval tv = { 0, 5000 };
        libusb_handle_events_timeout_completed(ctx_, &tv, 0);
        xfer_->poke();
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

//  for debugging
USBLink *lastUsbLink_;

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
    name_(vid + ":" + pid) {

    lastUsbLink_ = this;

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
    if (recvQ_.empty()) {
        oSize = 0;
        return 0;
    }
    Packet *p = recvQ_.front();
    oSize = p->size();
    return p->buffer();
}

void USBLink::end_receive(size_t size) {
    if (!recvQ_.empty() && (size || (recvQ_.front()->size() == 0))) {
        Packet *p = recvQ_.front();
        recvQ_.pop_front();
        p->destroy();
    }
}

size_t USBLink::queue_depth() {
    return xfer_->out_queue_depth();
}


