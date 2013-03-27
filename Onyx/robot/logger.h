
#if !defined(logger_h)
#define logger_h

enum LogKey {
    LogKeyNull = 0,
    LogKeyBattery = 1,
    LogKeyError = 2,
    LogKeyUSBOut = 3,
    LogKeyUSBIn = 4,
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

#endif
