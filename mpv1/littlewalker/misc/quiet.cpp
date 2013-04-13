
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libusb.h>

#include "../lib/defs.h"

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

    system("systemctl stop control");

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

    //  turn on pwm
    cmd[0] = (4 << 4);  //  CMD_PWMRATE
    cmd[1] = (0 >> 8) & 0xff;
    cmd[2] = 0 & 0xff;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 3, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error turning on PWM: %d (%s)\n", i, libusb_error_name(i));
        return 7;
    }
    fprintf(stderr, "turned off PWM\n");

    unsigned char bits = 0;
    unsigned char lookup[] = {
        1, 2, 4, 8, 4, 2
    };
    while (true) {
        cmd[0] = (2 << 4) | 2;
        cmd[1] = lookup[bits];
        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 2, &x, 1000);
        usleep(100000);
        bits = (bits + 1) % sizeof(lookup);
    }

    /*NOTREACHED*/
    return 0;
}

