#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include <libusb.h>
#include "../lib/usb.h"
#include "../lib/defs.h"

static libusb_context *ctx = 0;
static libusb_device_handle *dh = 0;
extern bool verbose;

void geterrcnt(char const *name) {
    int x = 0;
    unsigned char err = 0;
    int i = libusb_bulk_transfer(dh, DATA_INFO_EPNUM, &err, 1, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "%s: error getting DATA_INFO_EPNUM: %d (%s)\n", 
            name, i, libusb_error_name(i));
        exit(14);
    }
    if (err > 0) {
        fprintf(stderr, "%s: There have been %d errors since last check.\n", 
            name, err);
    }
    else {
        if (verbose) {
            fprintf(stderr, "%s: No errors so far.\n", name);
        }
    }
}

void init_usb() {
    int i = libusb_init(&ctx);
    if (i != 0) {
        fprintf(stderr, "libusb_init() failed\n");
        exit(1);
    }
    libusb_set_debug(ctx, 3);

    dh = libusb_open_device_with_vid_pid(ctx, 0xf000, 0x0002);
    if (!dh) {
        fprintf(stderr, "Could not find device 0xf000 / 0x0002\n");
        exit(2);
    }

    i = libusb_claim_interface(dh, 0);
    if (i != 0) {
        fprintf(stderr, "Could not claim interface 0: %d\n", i);
        exit(3);
    }

    unsigned char cmd[64];
    int x = 0;

    for (int i = 0; i < 5; ++i) {
        cmd[i*4+0] = (CMD_DDR << 4) | i;
        cmd[i*4+1] = 0;
        cmd[i*4+2] = (CMD_POUT << 4) | i;
        cmd[i*4+3] = 0;
    }
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 20, &x, 1000);
    if (i != 0) {
        fprintf(stderr, "Could not configure pin directions: %d\n", i);
        exit(4);
    }
    geterrcnt("configure pins");
    cmd[0] = (CMD_DDR << 4) | 0;
    cmd[1] = 0xff;
    cmd[2] = (CMD_POUT << 4) | 0;
    cmd[3] = 0;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 4, &x, 1000);
    if (i != 0) {
        fprintf(stderr, "Could not enable PORTB pins: %d\n", i);
        exit(4);
    }
    geterrcnt("enable PORTB");

    //  turn on pwm
    cmd[0] = (4 << 4);  //  CMD_PWMRATE
    cmd[1] = (PWM_FREQ >> 8) & 0xff;
    cmd[2] = PWM_FREQ & 0xff;
    send_usb(cmd, 3);
    geterrcnt("turn on PWM");
}

void send_usb(void const *data, size_t size) {
    int x;
    int i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, (unsigned char *)data, size, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "send_usb() failed: %d (%s)\n", i, libusb_error_name(i));
        exit(100);
    }
}

