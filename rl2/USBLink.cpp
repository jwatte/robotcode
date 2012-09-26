
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
    Transfer(unsigned char ep) : ep_(ep), xfer_(0), pack_(0) {
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
        libusb_fill_bulk_transfer(xfer_, dh, ep_, pack_->buffer(), pack_->max_size(),
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
    bool busy() const {
        return busy_;
    }
    Packet *ack_pack() {
        Packet *ret = pack_;
        pack_ = 0;
        return ret;
    }
    unsigned char ep_;
    libusb_transfer *xfer_;
    Packet *pack_;
    bool busy_;
};

boost::shared_ptr<Module> USBLink::open(boost::shared_ptr<Settings> const &set) {
    std::string vid("f000");
    std::string pid("0001");
    std::string endpoint("82");
    auto v = set->get_value("vid");
    if (!!v) {
        vid = v->get_string();
    }
    v = set->get_value("pid");
    if (!!v) {
        pid = v->get_string();
    }
    v = set->get_value("endpoint");
    if (!!v) {
        endpoint = v->get_string();
    }
    return boost::shared_ptr<Module>(new USBLink(vid, pid, endpoint));
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
        if (!xfer_->busy()) {
            if (xfer_->pack_) {
                pickup_.release();
                return_.acquire();
            }
            xfer_->recv(dh_);
        }
        else {
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

USBLink::USBLink(std::string const &vid, std::string const &pid, std::string const &endpoint) :
    vid_(vid),
    pid_(pid),
    endpoint_(endpoint),
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
    iep_ = (unsigned short)strtol(endpoint_.c_str(), &o, 16);
    xfer_ = new Transfer(iep_);
    dh_ = libusb_open_device_with_vid_pid(ctx_, ivid_, ipid_);
    if (!dh_ ) {
        throw std::runtime_error("Could not find USB device " + name_);
    }
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

void USBLink::set_board(unsigned char ix, boost::shared_ptr<Module> const &b) {
    if (ix > MAX_BOARD_INDEX) {
        throw std::runtime_error("Board index out of range in USBLink::set_board()");
    }
    if (boards_.size() <= ix) {
        boards_.insert(boards_.end(), ((int)ix+1-boards_.size()), 
            boost::shared_ptr<Board>());
    }
    boards_[ix] = b;
}

