
#if !defined(logger_h)
#define logger_h

enum LogKey {
    LogKeyNull = 0,
    LogKeyBattery = 1,
    LogKeyError = 2,
    NumLogKeys
};

struct logrec {
    unsigned long long time;
    double value;
    int key;
};

struct loghdr {
    char magic[8];
    unsigned long long time;
    unsigned long long recsize;
};

void open_logger();
void log(LogKey key, double value);
void flush_logger();

#endif
