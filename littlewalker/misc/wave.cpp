
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

    //  blink port D and B
    cmd[0] = (2 << 4) | 2;
    cmd[1] = 0xff;
    cmd[2] = (2 << 4) | 2;
    cmd[3] = 0;
    cmd[4] = (2 << 4) | 0;
    cmd[5] = 0xff;
    cmd[6] = (2 << 4) | 0;
    cmd[7] = 0;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 8, &x, 1000);
    if (i != 0) {
        fprintf(stderr, "Error blinking pins: %d (%s)\n", i, libusb_error_name(i));
        return 4;
    }
    fprintf(stderr, "blinked pins\n");
    geterrcnt(dh);

    //  turn on pwm
    cmd[0] = (4 << 4);  //  CMD_PWMRATE
    cmd[1] = (40000 >> 8) & 0xff;
    cmd[2] = 40000 & 0xff;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 3, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error turning on PWM: %d (%s)\n", i, libusb_error_name(i));
        return 7;
    }
    fprintf(stderr, "turned on PWM\n");
    geterrcnt(dh);

    while (true) {
        int pos = (rand() & 0xfff) + 1000;
        int n = 0;
        for (int i = 0; i < 8; ++i) {
            cmd[n++] = (5 << 4) | i;    //  CMD_SETPWM
            cmd[n++] = (pos >> 8) & 0xff;
            cmd[n++] = pos & 0xff;
        }
        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, n, &x, 1000);
        if (i < 0) {
            fprintf(stderr, "error writing PWM data: %d (%s)\n", i, libusb_error_name(i));
            return 8;
        }
        fprintf(stderr, "PWM %d\n", pos);
        geterrcnt(dh);
        usleep(1000000);
        cmd[0] = (2 << 4) | 2;  //  POUT port D
        cmd[1] = 0xff;
        cmd[2] = (6 << 4);  //  cmd wait
        cmd[3] = 0x10;      //  4 ms
        cmd[4] = 0x00;
        cmd[5] = (2 << 4) | 2;  //  POUT port D
        cmd[6] = 0;
        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 7, &x, 1000);
        if (i < 0) {
            fprintf(stderr, "error writing D blink data: %d (%s)\n", i, libusb_error_name(i));
            return 10;
        }
        geterrcnt(dh);
    }

    /*NOTREACHED*/
    return 0;
}

