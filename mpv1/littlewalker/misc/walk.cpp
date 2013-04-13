
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
    int x = 0;
    cmd[0] = (CMD_DDR << 4) | 0;
    cmd[1] = 0xff;
    cmd[2] = (CMD_POUT << 4) | 0;
    cmd[3] = 0;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 20, &x, 1000);
    if (i != 0) {
        fprintf(stderr, "Could not configure pin directions: %d\n", i);
        return 4;
    }
    fprintf(stderr, "configured pins\n");
    geterrcnt(dh);

    //  turn on pwm
    cmd[0] = (CMD_PWMRATE << 4);  //  CMD_PWMRATE
    cmd[1] = (40000 >> 8) & 0xff;
    cmd[2] = 40000 & 0xff;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 3, &x, 1000);
    if (i < 0) {
        fprintf(stderr, "error turning on PWM: %d (%s)\n", i, libusb_error_name(i));
        return 7;
    }
    fprintf(stderr, "turned on PWM\n");
    geterrcnt(dh);

    static struct {
        float left;
        float center;
        float right;
        int wait;
    }
    gait[] = {
        { -WALK_EXTENT, -LIFT_EXTENT, -WALK_EXTENT, 200000 },
        { WALK_EXTENT, -LIFT_EXTENT, WALK_EXTENT, 300000 },
        { WALK_EXTENT, LIFT_EXTENT, WALK_EXTENT, 200000 },
        { -WALK_EXTENT, LIFT_EXTENT, -WALK_EXTENT, 300000 },
    };
    size_t phase = -1;
    long delay = 0;
    float cleft = 0, ccenter = 0, cright = 0;
    while (true) {
        if (delay <= 0) {
            geterrcnt(dh);
            phase = phase + 1;
            if (phase >= sizeof(gait)/sizeof(gait[0])) {
                phase = 0;
            }
            delay = gait[phase].wait;
            printf("phase %ld delay %ld\n", (long)phase, (long)delay);
            printf("left %f\n", gait[phase].left);
        }
        cleft = cleft + (gait[phase].left - cleft) * 10000 / delay;
        ccenter = ccenter + (gait[phase].center - ccenter) * 10000 / delay;
        cright = cright + (gait[phase].right - cright) * 10000 / delay;
        unsigned short t = (unsigned short)(RIGHT_CENTER + cleft * 20);
        cmd[0] = (CMD_SETPWM << 4) | 0;
        cmd[1] = (t >> 8) & 0xff;
        cmd[2] = t & 0xff;
        t = (unsigned short)(CENTER_CENTER + ccenter * 20);
        cmd[3] = (CMD_SETPWM << 4) | 1;
        cmd[4] = (t >> 8) & 0xff;
        cmd[5] = t & 0xff;
        t = (unsigned short)(LEFT_CENTER + cright * 20);
        cmd[6] = (CMD_SETPWM << 4) | 2;
        cmd[7] = (t >> 8) & 0xff;
        cmd[8] = t & 0xff;
        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 9, &x, 1000);
        if (i < 0) {
            fprintf(stderr, "error stepping PWM: %d (%s)\n", i, libusb_error_name(i));
            return 7;
        }
        usleep(10000);
        delay -= 5000;
    }

    /*NOTREACHED*/
    return 0;
}

