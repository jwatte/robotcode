#if !defined(rl2_USBLink_h)
#define rl2_USBLink_h

#include "Module.h"
#include "semaphore.h"
#include <assert.h>
#include <boost/thread.hpp>

struct libusb_context;
struct libusb_device_handle;
class Settings;
class Transfer;

class Packet {
public:
    static inline Packet *create() { return new Packet(); }
    void destroy() { delete this; }
    void set_size(size_t sz) { assert(sz <= max_size()); size_ = sz; }
    unsigned char *buffer() { return data_; }
    size_t size() const { return size_; };
    size_t max_size() const { return sizeof(data_); }
private:
    unsigned char data_[256];
    size_t size_;
    Packet() : size_(0) {}
};


class USBLink : public Module {
public:
    static boost::shared_ptr<Module> open(boost::shared_ptr<Settings> const &set);
    void step();
    std::string const &name();

    virtual size_t num_properties();
    virtual boost::shared_ptr<Property> get_property_at(size_t ix);
    ~USBLink();
private:
    USBLink(std::string const &vid, std::string const &pid, std::string const &endpoint);
    void thread_fn();
    std::string vid_;
    std::string pid_;
    std::string endpoint_;
    unsigned short ivid_;
    unsigned short ipid_;
    unsigned int iep_;
    libusb_context *ctx_;
    libusb_device_handle *dh_;
    Transfer *xfer_;
    semaphore pickup_;
    semaphore return_;
    boost::shared_ptr<boost::thread> thread_;
    size_t inPackets_;
    size_t outPackets_;
    size_t errPackets_;
    boost::shared_ptr<Property> inPacketsProperty_;
    boost::shared_ptr<Property> outPacketsProperty_;
    boost::shared_ptr<Property> errPacketsProperty_;
    std::string name_;
};

#endif  //  rl2_USBLink_h

