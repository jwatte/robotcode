
#include "Services.h"
#include <assert.h>
#include <errno.h>


Services *gSvc;

Services::Services() {
    gSvc = this;
}

Services::~Services() {
    assert(gSvc == this);
    gSvc = NULL;
}

int Services::open(char const *str, int flags, int mode) {
    return ::open(str, flags, mode);
}

off_t Services::lseek(int fd, off_t pos, int whence) {
    return ::lseek(fd, pos, whence);
}

ssize_t Services::read(int fd, void *dst, ssize_t n) {
    return ::read(fd, dst, n);
}

ssize_t Services::write(int fd, void const *src, ssize_t n) {
    return ::write(fd, src, n);
}

int Services::fsync(int fd) {
    return ::fsync(fd);
}

int Services::close(int fd) {
    return ::close(fd);
}

int Services::error() {
    return errno;
}

