
#if !defined(util_h)
#define util_h

#include <string>
#include <string.h>

std::string hexnum(unsigned char const &ch);
std::string hexnum(unsigned short const &ch);
std::string hexnum(int ch);
unsigned char cksum(unsigned char const *a, size_t l);
double read_clock();
unsigned int fnv2_hash(void const *src, size_t sz);

template<size_t Sz>
void safecpy(char (&dst)[Sz], char const *src) {
    strncpy(dst, src, Sz);
    dst[Sz-1] = 0;
}

inline float cap(float f) {
    return (f < -1) ? -1 : (f > 1) ? 1 : f;
}
char const *next(char *&ptr, char const *end, char delim);



#endif  //  util_h
