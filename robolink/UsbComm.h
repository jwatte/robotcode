
#if !defined(UsbComm_h)
#define UsbComm_h

class UsbComm {
public:
    UsbComm(char const *name);
    ~UsbComm();
    bool open();
    void close();
    int read1();

    char name_[128];
    int fd_;

    void setup();
};

#endif  //  UsbComm_h

