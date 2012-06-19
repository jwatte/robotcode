
#if !defined(UsbComm_h)
#define UsbComm_h

#include <string>

class IReader {
public:
    virtual void setRTimestamp(double ts) = 0;
    virtual int read1() = 0;
};

class IWriter {
public:
    virtual void setWTimestamp(double ts) = 0;
    virtual void write1(int) = 0;
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

    char name_[128];
    int fd_;

    void setup();
};

#endif  //  UsbComm_h

