
#include "USBLink.h"
#include "Settings.h"
#include "PropertyImpl.h"
#include "Board.h"
#include <libusb.h>
#include <boost/bind.hpp>
#include <stdexcept>
#include <sstream>

#include "protocol.h"



class Transfer {
public:
    Transfer(libusb_device_handle dh, unsigned char iep, unsigned char oep) :
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
        libusb_fill_bulk_transfer(xfer_, dh_, ep_, pack_->buffer(), pack_->max_size(),
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
    unsigned char ep_;
    libusb_transfer *xfer_;
    Packet *pack_;
    bool busyRead_;
    bool busyWrite_;
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
    if (pickup_.nonblocking_available()) {
        pickup_.acquire();
        Packet *pack = xfer_->ack_pack();
        ++inPackets_;
        return_.release();
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
        inPacketsProperty_->set<long>(inPackets_);
        outPacketsProperty_->set<long>(outPackets_);
        errPacketsProperty_->set<long>(errPackets_);
        dropPacketsProperty_->set<long>(dropPackets_);
        errBytesProperty_->set<long>(errBytes_);
    }
}

void USBLink::thread_fn() {
    sched_param parm = { .sched_priority = 20 };
    if (pthread_setschedparam(pthread_self(), SCHED_RR, &parm) < 0) {
        std::string err(strerror(errno));
        std::cerr << "Camera::process(): pthread_setschedparam(): " << err << std::endl;
    }

    while (!thread_->interruption_requested()) {
        bool tosleep = true;
        if (!xfer_->busy_read()) {
            if (xfer_->pack_) {
                //  I should only do this once per packet
                pickup_.release();
                //  this will block until stepped from other side,
                //  which is bad for writing
                return_.acquire();
            }
            //  this should happen once I have the returned packet
            xfer_->recv();
            tosleep = false;
        }
        if (!xfer_->busy_write()) {
            boost::exclusive_lock<boost::mutex> lock(queueLock_);
            if (queue_.size()) {
                xfer_->write(*queue_.front());
                queue_.pop_front();
                tosleep = false;
            }
        }
        if (tosleep) {
            usleep(5000);
        }
        libusb_handle_events(ctx_);
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
        throw std::runtime_error("Too large return packet in USBLink::board_return()")
    }
    Packet *p = Packet::create();
    p->set_size(4 + sz);
    unsigned char *msg = p->buffer();
    msg[0] = 0xed;
    msg[1] = 'm';
    msg[2] = sz + 1;
    msg[3] = ix;
    memcpy(&msg[4], data, sz);
    boost::exclusive_lock<boost::mutex> lock(queueLock_);
    queue_.push_back(p);
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

