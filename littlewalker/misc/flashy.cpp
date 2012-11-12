
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libusb.h>


#define DATA_IN_EPNUM 0x82
#define DATA_OUT_EPNUM 0x03


int main() {
    libusb_context *ctx = 0;
    int i = libusb_init(&ctx);
    if (i != 0) {
        fprintf(stderr, "libusb_init() failed\n");
        return 1;
    }
    libusb_set_debug(ctx, 3);

    libusb_device_handle *dh = libusb_open_device_with_vid_pid(ctx, 0xf000, 0x0002);
    if (!dh) {
        fprintf(stderr, "Could not find device 0xf000 / 0x0002\n");
        return 2;
    }

    i = libusb_claim_interface(dh, 0);
    if (i != 0) {
        fprintf(stderr, "Could not claim interface 0: %d\n", i);
        return 3;
    }

    unsigned char cmd[64];
    for (int i = 0; i < 5; ++i) {
        cmd[i*2] = (1 << 4) | i;
        cmd[i*2 + 1] = 0xff;
    }
    int x = 0;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 10, &x, 1000);
    if (i != 0) {
        fprintf(stderr, "Could not configure pin directions: %d\n", i);
        return 4;
    }
    if (x != 10) {
        fprintf(stderr, "Short write (%d/10) on pin direction: %d\n", x, i);
        return 5;
    }
    fprintf(stderr, "configured pins\n");

    i = 0;
    while (true) {
        cmd[0] = (2 << 4) | 2;
        cmd[1] = cmd[1] ? 0 : 0x80;
        int e = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 2, &x, 1000);
        if (e != 0) {
            fprintf(stderr, "Could not set blink data: %d\n", e);
            return 4;
        }
        if (x != 2) {
            fprintf(stderr, "Short write (%d/10) on blink data: %d\n", x, e);
            return 5;
        }
        if (cmd[1]) {
            usleep(i * 100);
            i = i + 1;
            if (i == 256) {
                i = 0;
            }
        }
        else {
            usleep((255 - i) * 100);
        }
    }

    /*NOTREACHED*/
    return 0;
}

