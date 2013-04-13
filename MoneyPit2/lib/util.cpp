
#include "util.h"
#include <string.h>

unsigned char cksum(unsigned char const *a, size_t l) {
    unsigned char ck = 0;
    unsigned char const *b = a + l;
    while (a != b) {
        ck += *a;
        ++a;
    }
    return 255 - ck;
}

std::string hexnum(unsigned char const &ch) {
    char buf[5];
    snprintf(buf, 5, "0x%02x", ch);
    buf[4] = 0;
    return std::string(buf);
}

std::string hexnum(unsigned short const &ch) {
    char buf[7];
    snprintf(buf, 7, "0x%04x", ch);
    buf[6] = 0;
    return std::string(buf);
}

std::string hexnum(int ch) {
    char buf[11];
    snprintf(buf, 24, "0x%08x", (uint32_t)ch);
    buf[10] = 0;
    return std::string(buf);
}

double read_clock() {
    struct timespec ts;
    memset(&ts, 0, sizeof(ts));
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
}

unsigned int fnv2_hash(void const *src, size_t sz) {
    uint64_t hash = 14695981039346656037ULL; //  will cause truncation for 32-bit ints
    while (sz > 0) {
        hash = hash ^ *(unsigned char const *)src;
        hash = hash * 1099511628211ULL + 1001ULL;   //  avoid sticky-0 by adding a prime
        sz -= 1;
        src = (unsigned char const *)src + 1;
    }
    return (unsigned int)(hash ^ (hash >> 32)); //  avoid last-bit-stickiness
}

char const *next(char *&ptr, char const *end, char delim) {
    if (ptr == end) {
        return 0;
    }
    char const *ret = ptr;
    while (ptr < end) {
        if (*ptr == delim) {
            *ptr = 0;
            ++ptr;
            return ret;
        }
        ++ptr;
    }
    return ret;
}


