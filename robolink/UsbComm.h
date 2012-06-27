
#if !defined(UsbComm_h)
#define UsbComm_h

#include <string>

namespace boost {
    class thread;
}

class IReader {
public:
    virtual void setRTimestamp(double ts) = 0;
    virtual int read1() = 0;
};

class IWriter {
public:
    virtual void setWTimestamp(double ts) = 0;
    virtual void write1(int) = 0;
    virtual void writeImage(int ix, void const *data, size_t sz) = 0;
};

class UsbComm : public IReader {
public:
    UsbComm(std::string const &name);
    ~UsbComm();
    bool open();
    void close();

    void setRTimestamp(double ts);
    int read1();

    void message(unsigned char row, unsigned char col, std::string const &msg);
    void write_reg(unsigned char node, unsigned char reg, unsigned char n, void const *d);

    char name_[128];
    volatile int fd_;
    bool stalled_;
    enum { PIPE_SIZE = 1024 };
    volatile unsigned char pipe_[PIPE_SIZE];
    volatile unsigned int head_;
    volatile unsigned int tail_;
    boost::thread *thread_;

    void setup();
    void read_thread();
};

#endif  //  UsbComm_h

