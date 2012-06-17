
#if !defined(UsbComm_h)
#define UsbComm_h

#include <string>

class UsbComm {
public:
    UsbComm(std::string const &name);
    ~UsbComm();
    bool open();
    void close();
    int read1();
    void message(unsigned char row, unsigned char col, std::string const &msg);

    char name_[128];
    int fd_;

    void setup();
};

#endif  //  UsbComm_h

