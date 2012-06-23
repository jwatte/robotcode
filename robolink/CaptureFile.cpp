
#include "CaptureFile.h"

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <sys/mman.h>

#include <string>
#include <stdexcept>

#include <boost/bind.hpp>


#define BUF_SIZE (4096*1024)


CaptureFile::CaptureFile(std::string const &filename, bool writing) :
    name_(filename),
    writing_(writing),
    running_(false),
    writerCount_(0),
    readerCount_(BUF_SIZE),
    thread_(0),
    fd_(-1),
    readBase_(0),
    readSize_(0),
    readTimestamp_(0),
    readOffset_(0),
    scanBytes_(CF_Packet),
    wbuf_(0),
    head_(0),
    tail_(0),
    nbuf_(0),
    wTimestamp_(0)
{
    img_[0] = img_[1] = 0;
    size_[0] = size_[1] = 0;
    phys_[0] = phys_[1] = 0;
}

CaptureFile::~CaptureFile()
{
}

bool CaptureFile::open()
{
    if (fd_ != -1) {
        throw std::runtime_error("file already open in CaptureFile::open() " + name_);
    }
    fd_ = ::open(name_.c_str(), writing_ ? O_RDWR | O_CREAT | O_TRUNC : O_RDWR,
        0664);
    if (fd_ < 0) {
        throw std::runtime_error("Could not open capture file: " + name_);
    }
    if (writing_) {
        CF_Header hdr;
        hdr.what = CF_File;
        hdr.size = 0;
        hdr.timestamp = 0;
        if (::write(fd_, &hdr, sizeof(hdr)) != sizeof(hdr)) {
            throw std::runtime_error("Could not write file header: " + name_);
        }
        wbuf_ = malloc(BUF_SIZE);
        head_ = tail_ = 0;
        nbuf_ = 0;
        img_[0] = img_[1] = 0;
        size_[0] = size_[1] = 0;
        phys_[0] = phys_[1] = 0;
        running_ = true;
        thread_ = new boost::thread(boost::bind(&CaptureFile::write_thread,
            this));
    }
    else {
        readSize_ = lseek(fd_, 0, 2);
        readBase_ = mmap(0, readSize_, PROT_READ, MAP_SHARED, fd_, 0);
        if (readBase_ == MAP_FAILED) {
            throw std::runtime_error("Could not map file for read: " + name_);
        }
        CF_Header const *hdr = (CF_Header const *)readBase_;
        if (hdr->what != CF_File) {
            throw std::runtime_error("File is not a capture file: " + name_);
        }
        madvise(const_cast<void *>(readBase_), readSize_, MADV_WILLNEED);
        scanBytes_.attach(readBase_, readSize_);
    }
    return true;
}

void CaptureFile::setWTimestamp(double ts)
{
    flush();
    wTimestamp_ = ts;
}

void CaptureFile::write1(int i)
{
    if (nbuf_ == sizeof(buf_)) {
        throw std::runtime_error("Byte buffer write overflow: " + name_);
    }
    buf_[nbuf_++] = i;
}

void CaptureFile::writeImage(int index, void const *data, size_t size)
{
    assert(size < BUF_SIZE);
    if (index < 0 || index > 1) {
        throw std::runtime_error("writeImage() on bad index: " + name_);
    }
    if (size > 512*1024) {
        throw std::runtime_error("Image too big: " + name_);
    }
    if (phys_[index] < size) {
        img_[index] = realloc(img_[index], size + 10000);
        phys_[index] = size + 10000;
    }
    memcpy(img_[index], data, size);
    size_[index] = size;
}


void CaptureFile::setRTimestamp(double ts)
{
    readTimestamp_ = ts;
    //  todo: seek to the location
}

int CaptureFile::read1()
{
    //  todo: 
    return -1;
}

bool CaptureFile::readImage(int index, void const *&data, size_t &size)
{
    //  todo:
    return false;
}


void CaptureFile::flush()
{
    CF_Header hdr;
    hdr.what = CF_Packet;
    hdr.size = nbuf_ + size_[0] + size_[1] + sizeof(CF_PacketData);
    hdr.timestamp = wTimestamp_;
    if (hdr.size > 0) {
        size_t tsize = hdr.size + sizeof(hdr);
        readerCount_.wait(tsize);
        buf_write(&hdr, sizeof(hdr));
        CF_PacketData pd;
        pd.flags = 0;
        pd.bytes = nbuf_;
        if (nbuf_) {
            pd.flags |= CFF_HasBytes;
        }
        pd.image0 = size_[0];
        if (size_[0]) {
            pd.flags |= CFF_HasImage0;
        }
        pd.image1 = size_[1];
        if (size_[1]) {
            pd.flags |= CFF_HasImage1;
        }
        buf_write(&pd, sizeof(pd));
        if (nbuf_) {
            buf_write(buf_, nbuf_);
        }
        if (size_[0]) {
            buf_write(img_[0], size_[0]);
        }
        if (size_[1]) {
            buf_write(img_[1], size_[1]);
        }
        writerCount_.set(tsize);
    }
    nbuf_ = 0;
    size_[0] = size_[1] = 0;
}

void CaptureFile::write_thread()
{
    while (running_) {
        ssize_t w = writerCount_.wait_max();
        if (!running_) {
            break;
        }
        buf_flush(w, fd_);
    }
    std::cerr << "write_thread() terminating" << std::endl;
}

void CaptureFile::buf_write(void const *d, size_t n)
{
    assert(n < BUF_SIZE);
    while (n > 0) {
        size_t m = BUF_SIZE - (head_ & (BUF_SIZE - 1));
        if (m > n) {
            m = n;
        }
        memcpy((char *)wbuf_ + (head_ & (BUF_SIZE - 1)), d, m);
        d = (void const *)((char const *)d + m);
        head_ += m;
        n -= m;
    }
}

void CaptureFile::buf_flush(size_t n, int fd)
{
    assert(n < BUF_SIZE);
    while (n > 0) {
        size_t m = BUF_SIZE - (tail_ & (BUF_SIZE - 1));
        if (m > n) {
            m = n;
        }
        ssize_t s = ::write(fd, (char const *)wbuf_ + (tail_ & (BUF_SIZE - 1)), m);
        if (s != m) {
            int en = errno;
            throw std::runtime_error(name_ + " write error: " + strerror(en));
        }
        tail_ += m;
        n -= m;
    }
    fsync(fd);
}

HeaderScanner::HeaderScanner(unsigned int what) :
    what_(what),
    data_(0),
    offset_(0),
    size_(0)
{
}

void HeaderScanner::attach(void const *data, size_t size)
{
    data_ = data;
    offset_ = 0;
    size_ = size;
    next();
}

CF_Header const *HeaderScanner::next()
{
    while (true)
    {
        if (offset_ + sizeof(CF_Header) > size_)
        {
            return 0;
        }
        size_t sz = ((CF_Header const *)((char const *)data_ + offset_))->size;
        sz = (sz + 15) & ~15;
        if (offset_ + sizeof(CF_Header) + sz > size_)
        {
            return 0;
        }
        offset_ = offset_ + sizeof(CF_Header) + sz;
        if (((CF_Header const *)((char const *)data_ + offset_))->what == what_)
        {
            return (CF_Header const *)((char const *)data_ + offset_);
        }
    }
    return 0;
}

CF_Header const *HeaderScanner::current() const
{
    if (offset_ + sizeof(CF_Header) > size_)
    {
        return 0;
    }
    if (((CF_Header const *)((char const *)data_ + offset_))->what != what_)
    {
        return 0;
    }
    return (CF_Header const *)((char const *)data_ + offset_);
}

void const *HeaderScanner::data() const
{
    if (offset_ + sizeof(CF_Header) < size_)
    {
        return (char const *)data_ + offset_ + sizeof(CF_Header);
    }
    return 0;
}

