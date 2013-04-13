

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libusb.h>
#include "../lib/defs.h"
#include "../lib/usb.cpp"

bool verbose = true;

int main(int argc, char const *argv[]) {

    if (argc != 4) {
        fprintf(stderr, "usage: wrminmax channel min max\n");
        fprintf(stderr, "typical values of channel 0-5, min 600-1500, max 2000-5000\n");
        fprintf(stderr, "units are half-microseconds.\n");
        exit(1);
    }
    int channel = atoi(argv[1]);
    if (channel < 0 || channel > 7) {
        fprintf(stderr, "bad channel: %s\n", argv[1]);
        exit(2);
    }
    int min = atoi(argv[2]);
    if (min < 100 || min > 30000) {
        fprintf(stderr, "bad min: %s\n", argv[2]);
        exit(2);
    }
    int max = atoi(argv[3]);
    if (max <= min || max > 30000) {
        fprintf(stderr, "bad max: %s\n", argv[3]);
        exit(2);
    }
    init_usb();

    unsigned char cmd[10];
    cmd[0] = (unsigned char)(CMD_SETMINMAX << 4) | (unsigned char)channel;
    cmd[1] = (unsigned char)(min >> 8);
    cmd[2] = (unsigned char)(min & 0xff);
    cmd[3] = (unsigned char)(max >> 8);
    cmd[4] = (unsigned char)(max & 0xff);
    fprintf(stderr, "%02x %02x %02x %02x %02x\n", cmd[0], cmd[1], cmd[2], cmd[3], cmd[4]);
    send_usb(cmd, 5);

    geterrcnt("setminmax");

    fprintf(stderr, "ok\n");
    return 0;
}

