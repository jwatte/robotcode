
#if !defined(util_h)
#define util_h

#include <string>

std::string hexnum(unsigned char ch);
unsigned char cksum(unsigned char const *a, size_t l);
double read_clock();



#endif  //  util_h
