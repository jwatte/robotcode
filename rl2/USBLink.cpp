
#include "USBLink.h"
#include "Settings.h"
#include "PropertyImpl.h"
#include "Board.h"
#include <libusb.h>
#include <boost/bind.hpp>
#include <stdexcept>
#include <sstream>

#include "protocol.h"



/*
class Transfer {
public:
    Transfer(libusb_device_handle *dh, unsigned char iep, unsigned char oep) :
        dh_(dh),
        iep_(iep),
        oep_(oep),
        xfer_(0),
        pack_(0) {
        xfer_ = libusb_alloc_transfer(0);
        busyRead_ = false;
        busyWrite_ = false;
    }
    ~Transfer() {
        libusb_free_transfer(xfer_);
        if (pack_) {
            pack_->destroy();
        }
    }
    void recv() {
        pack_ = Packet::create();
        busyRead_ = true;
        xfer_->dev_handle = dh_;
        libusb_fill_bulk_transfer(xfer_, dh_, iep_, pack_->buffer(), pack_->max_size(),
                &Transfer::callback, this, 1000); //  a second is a long time!
        int err = libusb_submit_transfer(xfer_);
        if (err != 0) {
            std::cerr << "libusb_submit_transfer(): " << err << ": " << libusb_error_name(err) << std::endl;
            pack_->destroy();
            pack_ = 0;
            usleep(10000);
            busyRead_ = false;
        }
    }
    void write(Packet *p) {
        assert(!busyWrite_);
        busyWrite_ = true;
        ...     second transfer ...
    }
    static void callback(libusb_transfer *x) {
        ((Transfer *)x->user_data)->on_callback();
    }
    void on_callback() {
        pack_->set_size(xfer_->actual_length);
        busyRead_ = false;
    }
    bool busy_read() const {
        return busyRead_;
    }
    bool busy_write() const {
        return busyWrite_;
    }
    Packet *ack_pack() {
        Packet *ret = pack_;
        pack_ = 0;
        return ret;
    }
    libusb_device_handle *dh_;
    unsigned char iep_;
    unsigned char oep_;
    libusb_transfer *xfer_;
    Packet *pack_;
    bool busyRead_;
    bool busyWrite_;
};
*/

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
        if (!outPack_ && !outQueue_.empty()) {
            outPack_ = outQueue_.front();
            outQueue_.pop_front();
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
    }

    static void out_callback(libusb_transfer *cbArg) {
        reinterpret_cast<Transfer *>(cbArg->user_data)->out_complete();
    }

    void out_complete() {
        boost::unique_lock<boost::mutex> lock(lock_);
        outPack_->destroy();
        outPack_ = 0;
        start_out_inner();
    }

    //  must be called with lock held
    void start_in_inner() {
        if (!inPack_) {
            inPack_ = Packet::create();
            inXfer_->dev_handle = dh_;
            libusb_fill_bulk_transfer(inXfer_, dh_, oep_, inPack_->buffer(), inPack_->max_size(),
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


class USBReturn : public IReturn {
public:
    USBReturn(unsigned char board, USBLink *link) :
        board_(board),
        link_(link) {
    }
    virtual void set_data(unsigned char offset, void const *data, unsigned char sz) {
        link_->board_return(board_, offset, data, sz);
    }
    unsigned char board_;
    USBLink *link_;
};

boost::shared_ptr<Module> USBLink::open(boost::shared_ptr<Settings> const &set) {
    std::string vid("f000");
    std::string pid("0001");
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
        if (size == 0) {
            errPackets_++;
        }
        else {
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
                memcpy(&sendBuf_[sendBufEnd_], pack->buffer(), size);
                sendBufEnd_ += size;
                assert(sendBufEnd_ <= sizeof(sendBuf_));
            }
            unsigned int ptr = sendBufBegin_, end = sendBufEnd_;
            while (ptr != end) {
                if (sendBuf_[ptr] != BEGIN_PACKET) {
                    std::cerr << " " << std::hex << (int)sendBuf_[ptr];
                    ++errBytes_;
                    ++ptr;
                }
                else {
                    /* not enough data to fill a packet yet? */
                    if ((end - ptr < 3) ||
                        ((unsigned int)(end - ptr) < (unsigned int)(sendBuf_[ptr + 1] + 2))) {
                        break;
                    }
                    dispatch_cmd(&sendBuf_[ptr + 2], sendBuf_[ptr + 1]);
                    ptr += 2 + sendBuf_[ptr + 1];
                }
            }
            sendBufBegin_ = ptr;
        }
    }
    inPacketsProperty_->set<long>(inPackets_);
    outPacketsProperty_->set<long>(outPackets_);
    errPacketsProperty_->set<long>(errPackets_);
    dropPacketsProperty_->set<long>(dropPackets_);
    errBytesProperty_->set<long>(errBytes_);
}

void USBLink::thread_fn() {
    sched_param parm = { .sched_priority = 20 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "Camera::process(): pthread_setschedparam(): " << err << std::endl;
    }

    while (!thread_->interruption_requested()) {
        libusb_handle_events(ctx_);
        usleep(3000);
    }
    std::cerr << "USBLink::thread_fn() returning" << std::endl;
}

std::string const &USBLink::name() {
    return name_;
}

size_t USBLink::num_properties() {
    return 5;
}

boost::shared_ptr<Property> USBLink::get_property_at(size_t ix) {
    switch (ix) {
    case 0: return inPacketsProperty_;
    case 1: return outPacketsProperty_;
    case 2: return errPacketsProperty_;
    case 3: return dropPacketsProperty_;
    case 4: return errBytesProperty_;
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
static std::string str_err_packets("err_packets");
static std::string str_drop_packets("drop_packets");
static std::string str_err_bytes("err_bytes");

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
    errPackets_(0),
    dropPackets_(0),
    errBytes_(0),
    inPacketsProperty_(new PropertyImpl<long>(str_in_packets)),
    outPacketsProperty_(new PropertyImpl<long>(str_out_packets)),
    errPacketsProperty_(new PropertyImpl<long>(str_err_packets)),
    dropPacketsProperty_(new PropertyImpl<long>(str_drop_packets)),
    errBytesProperty_(new PropertyImpl<long>(str_err_bytes)),
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
    xfer_ = new Transfer(dh_, iep_, oep_);
    libusb_device_descriptor ldd;
    int er;
    er = libusb_get_device_descriptor(libusb_get_device(dh_), &ldd);
    if (er < 0) {
        throw std::runtime_error("Could not find USB descriptor for comm board " + 
            name_);
    }
    er = libusb_set_configuration(dh_, 1);
    if (er < 0) {
        throw std::runtime_error("Could not set configuration on comm board " +
            name_);
    }
    er = libusb_claim_interface(dh_, 0);
    if (er < 0) {
        throw std::runtime_error("Could not claim USB interface for comm board " +
            name_ + ". Is another process using it?");
    }
    thread_ = boost::shared_ptr<boost::thread>(new boost::thread(
        boost::bind(&USBLink::thread_fn, this)));
}

void USBLink::dispatch_cmd(unsigned char const *ptr, unsigned char sz) {
    if (*ptr == CMD_BOARD_UPDATE) {
        if (sz < 2) {
            std::cerr << "invalid board packet length" << std::endl;
            ++errPackets_;
            return;
        }
        dispatch_board(ptr[1], ptr+2, sz - 2);
        return;
    }
    std::stringstream ss;
    for (unsigned char ch = 0; ch < sz; ++ch) {
        ss << " " << std::hex << std::right << std::setw(2) << "0x" << (int)ptr[ch];
    }
    std::cout << ss.str() << std::endl;
}

void USBLink::dispatch_board(unsigned char board, unsigned char const *info, unsigned char sz) {
    boost::shared_ptr<Module> brd;
    switch (board) {
        case MOTOR_BOARD:
        case SENSOR_BOARD:
        case USB_BOARD:
        case IMU_BOARD:
            if (board < boards_.size()) {
                brd = boards_[board];
                if (!!brd) {
                    brd->cast_as<Board>()->on_data(info, sz);
                }
            }
            break;
        default:
            std::cerr << "unknown board identifier in packet" << std::endl;
            ++errPackets_;
            break;
    }
}

void USBLink::board_return(unsigned char ix, unsigned char offset, void const *data, unsigned char sz) {
    if (sz > 30) {
        throw std::runtime_error("Too large return packet in USBLink::board_return()");
    }
    unsigned char msg[4 + 30];
    msg[0] = 0xed;
    msg[1] = 'm';
    msg[2] = sz + 1;
    msg[3] = ix;
    memcpy(&msg[4], data, sz);
    xfer_->out_write(msg, 4 + sz);
    ++outPackets_;
}

void USBLink::set_board(unsigned char ix, boost::shared_ptr<Module> const &b) {
    if (ix > MAX_BOARD_INDEX) {
        throw std::runtime_error("Board index out of range in USBLink::set_board()");
    }
    if (boards_.size() <= ix) {
        boards_.insert(boards_.end(), ((int)ix+1-boards_.size()), 
            boost::shared_ptr<Board>());
    }
    boards_[ix] = b;
    b->set_return(boost::shared_ptr<IReturn>(new USBReturn(ix, this)));
}

