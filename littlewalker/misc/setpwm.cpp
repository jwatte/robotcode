

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

    if (argc != 3) {
        fprintf(stderr, "usage: setpwm channel value\n");
        fprintf(stderr, "typical values of channel 0-5, value 1000-5000\n");
        fprintf(stderr, "units are half-microseconds.\n");
        exit(1);
    }
    int channel = atoi(argv[1]);
    if (channel < 0 || channel > 7) {
        fprintf(stderr, "bad channel: %s\n", argv[1]);
        exit(2);
    }
    int value = atoi(argv[2]);
    if (value < 100 || value > 30000) {
        fprintf(stderr, "bad value: %s\n", argv[2]);
        exit(2);
    }
    init_usb();

    unsigned char cmd[10];
    cmd[0] = (unsigned char)(CMD_SETPWM << 4) | (unsigned char)channel;
    cmd[1] = (unsigned char)(value >> 8);
    cmd[2] = (unsigned char)(value & 0xff);
    send_usb(cmd, 3);

    geterrcnt("setpwm");

    fprintf(stderr, "ok\n");
    return 0;
}

