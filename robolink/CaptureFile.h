#if !defined(CaptureFile_h)
#define CaptureFile_h

#include "UsbComm.h"
#include "Signal.h"

enum
{
    CF_File = 0x987f3200,
    CF_Packet = 0x01010101,
};

struct CF_Header
{
    unsigned int what;
    unsigned int size;  //  round up to 16 bytes for finding next header
    double timestamp;
};

enum
{
    CFF_HasBytes = 0x1,
    CFF_HasImage0 = 0x2,
    CFF_HasImage1 = 0x4
};

struct CF_PacketData
{
    unsigned int flags;
    unsigned int bytes;
    unsigned int image0;
    unsigned int image1;
};

class HeaderScanner
{
public:
    HeaderScanner(unsigned int what);
    void attach(void const *data, size_t size);
    CF_Header const *next();
    CF_Header const *current() const;
    void const *data() const;

    unsigned int what_;
    void const *data_;
    size_t offset_;
    size_t size_;
};

class CaptureFile : public IReader, public IWriter
{
public:
    CaptureFile(std::string const &filename, bool writing);
    ~CaptureFile();
    bool open();

    void setWTimestamp(double ts);
    void write1(int i);
    void writeImage(int index, void const *data, size_t size);

    void setRTimestamp(double ts);
    int read1();
    bool readImage(int index, void const *&data, size_t &size);

    std::string name_;
    bool writing_;
    volatile bool running_;
    Semaphore writerCount_;
    Semaphore readerCount_;
    boost::thread *thread_;
    int fd_;
    void const *readBase_;
    size_t readSize_;
    double readTimestamp_;
    size_t readOffset_;
    HeaderScanner scanBytes_;

    void *wbuf_;
    volatile unsigned int head_;
    volatile unsigned int tail_;
    char buf_[4096];
    size_t nbuf_;
    void *img_[2];
    size_t size_[2];
    size_t phys_[2];
    double wTimestamp_;

    void flush();
    void write_thread();
    void buf_write(void const *d, size_t n);
    void buf_flush(size_t n, int fd);
};

#endif  //  CaptureFile_h

