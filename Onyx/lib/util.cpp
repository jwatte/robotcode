
#include "util.h"

unsigned char cksum(unsigned char const *a, size_t l) {
    unsigned char ck = 0;
    unsigned char const *b = a + l;
    while (a != b) {
        ck += *a;
        ++a;
    }
    return 255 - ck;
}

std::string hexnum(unsigned char ch) {
    char buf[5];
    snprintf(buf, 5, "0x%02x", ch);
    buf[4] = 0;
    return std::string(buf);
}


