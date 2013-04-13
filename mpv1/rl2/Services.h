#if !defined (rl2_Services_h)
#define rl2_Services_h

#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

/*  Use services for file interface and other physics things, 
    to enable unit testing. */
class Services {
public:

    Services();
    virtual ~Services();
    virtual int open(char const *str, int flags, int mode = 0);
    virtual off_t lseek(int fd, off_t pos, int whence);
    virtual ssize_t read(int fd, void *dst, ssize_t n);
    virtual ssize_t write(int fd, void const *src, ssize_t n);
    virtual int fsync(int fd);
    virtual int close(int fd);
    virtual int error();
};
extern Services *gSvc;

#endif

