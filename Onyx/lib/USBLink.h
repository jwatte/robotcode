#if !defined(rl2_USBLink_h)
#define rl2_USBLink_h

#include "Module.h"
#include "semaphore.h"
#include <assert.h>
#include <boost/thread.hpp>
#include <deque>

struct libusb_context;
struct libusb_device_handle;
class Settings;
class Transfer;
class Board;

class Packet {
public:
    static inline Packet *create() { return new Packet(); }
    void destroy() { delete this; }
    void set_size(size_t sz) { assert(sz <= max_size()); size_ = sz; }
    unsigned char *buffer() { return data_; }
    size_t size() const { return size_; };
    size_t max_size() const { return sizeof(data_); }
private:
    unsigned char data_[128];
    size_t size_;
    Packet() : size_(0) {}
};

class USBReceiver {
public:
    virtual size_t on_data(unsigned char const *info, size_t sz) = 0;
};


class USBLink : public cast_as_impl<Module, USBLink> {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
    ~USBLink();
    void step();
    std::string const &name();

    virtual size_t num_properties();
    virtual boost::shared_ptr<Property> get_property_at(size_t ix);
    void raw_send(void const *data, unsigned char sz);
    unsigned char const *begin_receive(size_t &oSize);
    void end_receive(size_t sz);
    size_t queue_depth();
private:
    friend class USBReturn;
    USBLink(std::string const &vid, std::string const &pid,
        std::string const &ep_input, std::string const &ep_output);
    void thread_fn();

    std::string vid_;
    std::string pid_;
    std::string ep_input_;
    std::string ep_output_;
    unsigned short ivid_;
    unsigned short ipid_;
    unsigned int iep_;
    unsigned int oep_;
    libusb_context *ctx_;
    libusb_device_handle *dh_;
    Transfer *xfer_;
    semaphore pickup_;
    semaphore return_;
    boost::mutex queueLock_;
    std::deque<Packet *> queue_;
    boost::shared_ptr<boost::thread> thread_;
    size_t inPackets_;
    size_t outPackets_;
    size_t dropPackets_;
    boost::shared_ptr<Property> inPacketsProperty_;
    boost::shared_ptr<Property> outPacketsProperty_;
    boost::shared_ptr<Property> dropPacketsProperty_;
    unsigned char sendBuf_[1024];
    unsigned int sendBufBegin_;
    unsigned int sendBufEnd_;
    std::string name_;
};

#endif  //  rl2_USBLink_h

