
#if !defined(logger_h)
#define logger_h

#include <stdlib.h>

enum LogKey {
    LogKeyNull = 0,
    LogKeyBattery = 1,
    LogKeyError = 2,
    LogKeyUSBOut = 3,
    LogKeyUSBIn = 4,
    LogKeyTemperature = 5,
    NumLogKeys
};

struct logrec {
    unsigned long long time;
    int key;
    int size;
    double value;
};

struct loghdr {
    char magic[8];
    unsigned long long time;
    unsigned long long recsize;
};

void open_logger();
void log(LogKey key, double value);
void log(LogKey key, void const *data, unsigned long size);
void flush_logger();
void log_ratelimit(LogKey key, bool limit);

bool logger_open_read(char const *file);
bool logger_read_next(logrec *header, void const **data, size_t *size);
bool logger_close_read();

#endif
