
#include "logger.h"
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static FILE *f;
static int nwr;
static loghdr hdr;
static time_t lastflush;

static time_t lastwrite[NumLogKeys];

static void close_logger() {
    if (f) {
        fclose(f);
    }
}

void open_logger() {
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
        atexit(&close_logger);
    }
}

void log(LogKey key, double value) {
    if (f) {
        time_t t;
        time(&t);
        //  log at most one value per second
        if (key >= NumLogKeys || lastwrite[key] != t) {
            if (key < NumLogKeys) {
                lastwrite[key] = t;
            }
            logrec lr;
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

void flush_logger() {
    if (f) {
        fflush(f);
        nwr = 0;
        time(&lastflush);
    }
}

