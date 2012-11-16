
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <libusb.h>

#define RIGHT_CENTER 3200
#define LEFT_CENTER 3200
#define CENTER_CENTER 3100

//  in half-microseconds
#define PWM_FREQ 30000

#define WALK_EXTENT 28
#define LIFT_EXTENT 40


#define DATA_INFO_EPNUM 0x81
#define DATA_IN_EPNUM 0x82
#define DATA_OUT_EPNUM 0x03

#define CMD_DDR 1
#define CMD_POUT 2
#define CMD_PIN 3
#define CMD_TWOBYTEARG 4
#define CMD_PWMRATE CMD_TWOBYTEARG
#define CMD_SETPWM 5
#define CMD_WAIT 6
#define CMD_LERPPWM 7



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
    cmd[0] = (4 << 4);  //  CMD_PWMRATE
    cmd[1] = (PWM_FREQ >> 8) & 0xff;
    cmd[2] = PWM_FREQ & 0xff;
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
        int delay;
    }
    gait[] = {
        { -WALK_EXTENT, -LIFT_EXTENT, -WALK_EXTENT, 200000 },
        { WALK_EXTENT, -LIFT_EXTENT, WALK_EXTENT, 300000 },
        { WALK_EXTENT, LIFT_EXTENT, WALK_EXTENT, 200000 },
        { -WALK_EXTENT, LIFT_EXTENT, -WALK_EXTENT, 300000 },
    };
    size_t phase = -1;
    long delay = 0;
    while (true) {
        phase = phase + 1;
        if (phase >= sizeof(gait)/sizeof(gait[0])) {
            phase = 0;
        }
        delay = gait[phase].delay * 4;
        printf("phase %ld delay %ld\n", (long)phase, (long)delay);
        printf("left %f\n", gait[phase].left);
        unsigned short t = (unsigned short)(RIGHT_CENTER + gait[phase].right * 20);
        int n = 0;
        cmd[n++] = (CMD_LERPPWM << 4) | 0;
        cmd[n++] = (t >> 8) & 0xff;
        cmd[n++] = t & 0xff;
        cmd[n++] = delay / PWM_FREQ;
        t = (unsigned short)(CENTER_CENTER + gait[phase].center * 20);
        cmd[n++] = (CMD_LERPPWM << 4) | 1;
        cmd[n++] = (t >> 8) & 0xff;
        cmd[n++] = t & 0xff;
        cmd[n++] = delay / PWM_FREQ;
        t = (unsigned short)(LEFT_CENTER + gait[phase].left * 20);
        cmd[n++] = (CMD_LERPPWM << 4) | 2;
        cmd[n++] = (t >> 8) & 0xff;
        cmd[n++] = t & 0xff;
        cmd[n++] = delay / PWM_FREQ;
        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, n, &x, 1000);
        if (i < 0) {
            fprintf(stderr, "error stepping PWM: %d (%s)\n", i, libusb_error_name(i));
            return 7;
        }
        geterrcnt(dh);
        usleep(delay / 2);
    }

    /*NOTREACHED*/
    return 0;
}

