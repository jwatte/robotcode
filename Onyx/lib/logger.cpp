
#include "logger.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <boost/thread.hpp>



static FILE *f;
static int nwr;
static loghdr hdr;
static time_t lastflush;

static time_t lastwrite[NumLogKeys];
static bool ratelimit[NumLogKeys];
static bool installed = false;

static boost::recursive_mutex mt;

static void close_logger() {
    boost::unique_lock<boost::recursive_mutex> l(mt);
    if (f) {
        fclose(f);
        f = 0;
    }
}

void open_logger() {
    boost::unique_lock<boost::recursive_mutex> l(mt);
    time_t now;
    time(&now);
    struct tm t = *localtime(&now);
    char buf[100];
    strftime(buf, 100, "/var/robot/log/%Y-%m-%d_%H.%M.%S", &t);
    f = fopen(buf, "wb");
    if (f) {
        strncpy(hdr.magic, "logfile\n", 8);
        hdr.time = now;
        hdr.recsize = sizeof(logrec);
        fwrite(&hdr, 1, sizeof(hdr), f);
        lastflush = hdr.time - 1;
        for (size_t i = 0; i != NumLogKeys; ++i) {
            lastwrite[i] = hdr.time - 1;
        }
        if (!installed) {
            atexit(&close_logger);
            installed = true;
        }
        memset(ratelimit, 1, sizeof(ratelimit));
    }
}

void log_ratelimit(LogKey key, bool limit) {
    if (key >= NumLogKeys) {
        throw std::runtime_error("key must be defined in log_ratelimit()");
    }
    ratelimit[key] = limit;
}

void log(LogKey key, double value) {
    if (key >= NumLogKeys) {
        throw std::runtime_error("key must be defined in log(value)");
    }
    boost::unique_lock<boost::recursive_mutex> l(mt);
    if (f) {
        time_t t;
        time(&t);
        //  log at most one value per second
        if (lastwrite[key] != t || !ratelimit[key]) {
            lastwrite[key] = t;
            logrec lr;
            lr.size = sizeof(double);
            lr.time = t;
            lr.value = value;
            lr.key = key;
            fwrite(&lr, 1, sizeof(lr), f);
            ++nwr;
            if (nwr >= 100 || lr.time - lastflush >= 60) {
                flush_logger();
            }
        }
    }
}

void log(LogKey key, void const *data, unsigned long size) {
    if (key >= NumLogKeys) {
        throw std::runtime_error("key must be defined in log(data)");
    }
    boost::unique_lock<boost::recursive_mutex> l(mt);
    if (f) {
        time_t t;
        time(&t);
        //  log at most one value per second
        if (lastwrite[key] != t || !ratelimit[key]) {
            lastwrite[key] = t;
            logrec lr;
            lr.size = (int)size;
            lr.time = t;
            lr.key = key;
            fwrite(&lr, 1, sizeof(lr) - sizeof(double), f);
            fwrite(data, 1, size, f);
            ++nwr;
            if (nwr >= 100 || lr.time - lastflush >= 60) {
                flush_logger();
            }
        }
    }
}

void flush_logger() {
    boost::unique_lock<boost::recursive_mutex> l(mt);
    if (f) {
        fflush(f);
        nwr = 0;
        time(&lastflush);
    }
}


static std::vector<char> f_read;
static char const *ptr;
static char const *end;

bool logger_open_read(char const *file) {
    logger_close_read();
    FILE *fr = fopen(file, "rb");
    if (!fr) {
        fprintf(stderr, "%s: not found\n", file);
        return false;
    }
    fseek(fr, 0, 2);
    unsigned long l = ftell(fr);
    if (l < sizeof(loghdr)) {
        fprintf(stderr, "%s: short file\n", file);
        fclose(fr);
        return false;
    }
    fseek(fr, 0, 0);
    f_read.resize(l);
    fread(&f_read[0], 1, l, fr);
    fclose(fr);
    ptr = &f_read[0];
    if (strncmp(ptr, "logfile\n", 8) || 
        ((loghdr *)ptr)->recsize != sizeof(logrec)) {
        fprintf(stderr, "%s: unknown file format\n", file);
        logger_close_read();
        return false;
    }
    end = ptr + l;
    ptr += sizeof(loghdr);
    return true;
}

bool logger_read_next(logrec *header, void const **data, size_t *size) {
    if (!ptr || ptr == end) {
        return false;
    }
    if ((size_t)(end - ptr) < sizeof(logrec) - sizeof(double)) {
        end = ptr;  //  detect premature end
        return false;
    }
    if ((size_t)(end - ptr) < sizeof(logrec) + ((logrec *)ptr)->size) {
        end = ptr;
        return false;   //  detect premature end
    }
    memcpy(header, ptr, sizeof(logrec));
    memcpy(&header->value, ptr + sizeof(logrec) - sizeof(double), 
        std::min(header->size, (int)sizeof(double)));
    *data = ptr + sizeof(logrec) - sizeof(double);
    *size = header->size;
    ptr += sizeof(logrec) - sizeof(double) + *size;
    return true;
}

bool logger_close_read() {
    ptr = 0;
    std::vector<char>().swap(f_read);
    return true;
}

