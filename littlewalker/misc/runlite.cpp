
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libusb.h>


#define DATA_INFO_EPNUM 0x81
#define DATA_IN_EPNUM 0x82
#define DATA_OUT_EPNUM 0x03


void geterrcnt(libusb_device_handle *dh) {
    int x = 0;
    unsigned char err = 0;
    int i = libusb_bulk_transfer(dh, DATA_INFO_EPNUM, &err, 1, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error getting DATA_INFO_EPNUM: %d (%s)\n", i, libusb_error_name(i));
        exit(14);
    }
    if (err > 0) {
        fprintf(stderr, "There have been %d errors since last check.\n", err);
    }
    else {
        fprintf(stderr, "No errors so far.\n");
    }
}

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
        cmd[i*4] = (1 << 4) | i;        //  CMD_DDR
        cmd[i*4 + 1] = 0xff;
        cmd[i*4 + 2] = (2 << 4) | i;    //  CMD_POUT
        cmd[i*4 + 3] = 0;
    }
    int x = 0;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 20, &x, 1000);
    if (i != 0) {
        fprintf(stderr, "Could not configure pin directions: %d\n", i);
        return 4;
    }
    fprintf(stderr, "configured pins\n");
    geterrcnt(dh);

    unsigned char bits = 1;
    char dir = 1;
    while (true) {
        cmd[0] = (2 << 4) | 2;
        cmd[1] = bits;
        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 2, &x, 1000);
        usleep(100000);
        if (bits == 8) {
            dir = -1;
        }
        else if (bits == 1) {
            dir = 1;
        }
        if (dir > 0) {
            bits = bits << 1;
        }
        else {
            bits = bits >> 1;
        }
    }

    /*NOTREACHED*/
    return 0;
}

