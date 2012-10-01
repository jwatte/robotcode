#if !defined(rl2_FakeServices_h)
#define rl2_FakeServices_h

#include "Services.h"
#include <string>
#include <vector>
#include <map>

class FakeServices : public Services {

public:

    std::map<std::string, std::vector<char>> fakeFiles_;
    std::map<int, std::pair<std::string, off_t>> fakeFds_;

    FakeServices();
    void clear();

    virtual int open(char const *str, int flags, int mode = 0);
    virtual off_t lseek(int fd, off_t pos, int whence);
    virtual ssize_t read(int fd, void *dst, ssize_t n);
    virtual ssize_t write(int fd, void const *src, ssize_t n);
    virtual int fsync(int fd);
    virtual int close(int fd);
    virtual int error();

    int error_;
};

#endif  //  rl2_FakeServices_h
