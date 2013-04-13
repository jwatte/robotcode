#include <logger.h>
#include <stdio.h>
#include <stdlib.h>


bool battery = false;
bool error = false;
bool usbout = false;
bool usbin = false;
bool temperature = false;
bool header = false;
bool all = false;
bool got = false;

void usage() {
    fprintf(stderr, "usage: logdump [-beoitha] file ...\n");
    fprintf(stderr, " -b     battery level\n");
    fprintf(stderr, " -e     errors\n");
    fprintf(stderr, " -o     usb out\n");
    fprintf(stderr, " -i     usb in\n");
    fprintf(stderr, " -t     temperature\n");
    fprintf(stderr, " -h     header\n");
    fprintf(stderr, " -a     all\n");
    exit(1);
}


void format_battery(logrec const *rec, void const *data, size_t size) {
    fprintf(stdout, "battery, %.1f\n", rec->value);
}

void format_error(logrec const *rec, void const *data, size_t size) {
    fprintf(stdout, "error, \"%.*s\"\n", (int)size, (char const *)data);
}

void format_usbout(logrec const *rec, void const *data, size_t size) {
    fprintf(stdout, "usbout, %d: ", (int)size);
    for (size_t i = 0; i != size; ++i) {
        fprintf(stdout, "0x%02x ", ((unsigned char *)data)[i]);
    }
    fprintf(stdout, "\n");
}

void format_usbin(logrec const *rec, void const *data, size_t size) {
    fprintf(stdout, "usbin, %d: ", (int)size);
    for (size_t i = 0; i != size; ++i) {
        fprintf(stdout, "0x%02x ", ((unsigned char *)data)[i]);
    }
    fprintf(stdout, "\n");
}

void format_temperature(logrec const *rec, void const *data, size_t size) {
    fprintf(stdout, "temp,");
    for (size_t i = 0; i != size; ++i) {
        fprintf(stdout, " %d", ((unsigned char *)data)[i]);
    }
    fprintf(stdout, "\n");
}

void parse_options(int &argc, char const **&argv) {
    while (argv[1]) {
        if (argv[1][0] == '-') {
            char const *arg = argv[1];
            for (int i = 1; arg[i]; ++i) {
                switch (arg[i]) {
                case 'b':   //  battery
                    battery = true;
                    got = true;
                    break;
                case 'e':   //  error
                    error = true;
                    got = true;
                    break;
                case 'o':   //  usbout
                    usbout = true;
                    got = true;
                    break;
                case 'i':   //  usbin
                    usbin = true;
                    got = true;
                    break;
                case 't':   //  temperature
                    temperature = true;
                    got = true;
                    break;
                case 'h':   //  header
                    header = true;
                    got = true;
                    break;
                case 'a':   //  all
                    all = true;
                    got = true;
                    break;
                default:
                    usage();
                }
            }
        }
        else {
            break;
        }
        --argc;
        ++argv;
    }
    if (!got) {
        all = true;
    }
}

int main(int argc, char const *argv[]) {
    parse_options(argc, argv);

    long nitems = 0;
    long nfiles = 0;
    if (header) {
        fprintf(stdout, "nfile, nfileitem, nitem, time, type, data\n");
    }

    while (argv[1]) {
        loghdr lh;
        if (!logger_open_read(argv[1], &lh)) {
            fprintf(stderr, "logger: could not open: %s\n", argv[1]);
            exit(1);
        }
        long fileitem = 0;
        logrec lr;
        void const *data;
        size_t size;
        while (logger_read_next(&lr, &data, &size)) {
            void (*ffunc)(logrec const *, void const *, size_t) = 0;
            if (lr.key == LogKeyBattery && (all || battery)) {
                ffunc = &format_battery;
            }
            if (lr.key == LogKeyError && (all || error)) {
                ffunc = &format_error;
            }
            if (lr.key == LogKeyUSBOut && (all || usbout)) {
                ffunc = &format_usbout;
            }
            if (lr.key == LogKeyUSBIn && (all || usbin)) {
                ffunc = &format_usbin;
            }
            if (lr.key == LogKeyTemperature && (all || temperature)) {
                ffunc = &format_temperature;
            }
            if (header && ffunc != 0) {
                fprintf(stdout, "%ld, %ld, %ld, ", nfiles, nitems, fileitem);
            }
            if (ffunc != 0) {
                fprintf(stdout, "%lld, ", lr.time - lh.time);
                (*ffunc)(&lr, data, size);
            }
            ++nitems;
            ++fileitem;
        }
        logger_close_read();
        ++nfiles;
        ++argv;
        --argc;
    }
    return 0;
}

