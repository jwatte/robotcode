
#include "FakeServices.h"
#include <stdio.h>
#include <string.h>

FakeServices::FakeServices() {
    clear();
}

void FakeServices::clear() {
    fakeFiles_.clear();
    fakeFiles_["stdin"] = std::vector<char>();
    fakeFiles_["stdout"] = std::vector<char>();
    fakeFiles_["stderr"] = std::vector<char>();
    fakeFds_.clear();
    fakeFds_[0].first = "stdin";
    fakeFds_[0].second = 0;
    fakeFds_[1].first = "stdout";
    fakeFds_[1].second = 0;
    fakeFds_[2].first = "stderr";
    fakeFds_[2].second = 0;
}

int FakeServices::open(char const *str, int flags, int mode) {
    error_ = 0;
    if ((flags & O_CREAT) == O_CREAT) {
        //  make sure it exists
        fakeFiles_[str];
    }
    if ((flags & O_TRUNC) == O_TRUNC) {
        fakeFiles_[str] = std::vector<char>();
    }
    if (fakeFiles_.find(str) == fakeFiles_.end()) {
        error_ = ENOENT;
        return -1;
    }
    for (int i = 0; i < 1024; ++i) {
        if (fakeFds_.find(i) == fakeFds_.end()) {
            fakeFds_[i].first = str;
            fakeFds_[i].second = 0;
            return i;
        }
    }
    error_ = EMFILE;
    return -1;
}

off_t FakeServices::lseek(int fd, off_t pos, int whence) {
    error_ = 0;
    if (fakeFds_.find(fd) == fakeFds_.end()) {
        error_ = EBADF;
        return -1;
    }
    switch (whence) {
        case 0: pos = pos; break;
        case 1: pos = pos + fakeFds_[fd].second; break;
        case 2: pos = pos + fakeFiles_[fakeFds_[fd].first].size(); break;
        default: error_ = EINVAL; return -1;
    }
    fakeFds_[fd].second = pos;
    return pos;
}

ssize_t FakeServices::read(int fd, void *dst, ssize_t n) {
    error_ = 0;
    if (fakeFds_.find(fd) == fakeFds_.end()) {
        error_ = EBADF;
        return -1;
    }
    off_t pos = fakeFds_[fd].second;
    off_t end = fakeFiles_[fakeFds_[fd].first].size();
    if (pos >= end) {
        return 0;
    }
    if (n > end - pos) {
        n = end - pos;
    }
    memcpy(dst, &fakeFiles_[fakeFds_[fd].first][pos], n);
    return n;
}

ssize_t FakeServices::write(int fd, void const *src, ssize_t n) {
    error_ = 0;
    if (fakeFds_.find(fd) == fakeFds_.end()) {
        error_ = EBADF;
        return -1;
    }
    if (n == 0) {
        return 0;
    }
    off_t pos = fakeFds_[fd].second;
    if (pos + n > 16 * 1024 * 1024) {
        error_ = ENOSPC;
        return -1;
    }
    if ((off_t)fakeFiles_[fakeFds_[fd].first].size() < (off_t)(pos + n)) {
        fakeFiles_[fakeFds_[fd].first].resize(pos + n, 0);
    }
    memcpy(&fakeFiles_[fakeFds_[fd].first][pos], src, n);
    fakeFds_[fd].second = pos + n;
    return n;
}

int FakeServices::fsync(int fd) {
    return 0;
}

int FakeServices::close(int fd) {
    error_ = 0;
    if (fakeFds_.find(fd) == fakeFds_.end()) {
        error_ = EBADF;
        return -1;
    }
    fakeFds_.erase(fakeFds_.find(fd));
    return 0;
}

int FakeServices::error() {
    return error_;
}


