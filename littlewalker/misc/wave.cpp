
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
    if (x != 20) {
        fprintf(stderr, "Short write (%d/10) on pin direction: %d\n", x, i);
        return 5;
    }
    fprintf(stderr, "configured pins\n");

    cmd[0] = (2 << 4) | 2;
    cmd[1] = 0xff;
    cmd[2] = (2 << 4) | 2;
    cmd[3] = 0;
    i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, 4, &x, 1000);
    fprintf(stderr, "blinked pins\n");

    while (true) {

        int n = 0;
        cmd[n++] = (4 << 4) | 2;  //  CMD_WAVE, port D
        //  format is "port bitmask, delay tick count-1"
        //  Each tick is 0.5 microseconds, max packet size is 64 bytes, 
        //  so 31*256/2 us is the maximum duration -- a little under 4 ms.
        //  with specific times for specific pins, the max duration will 
        //  be less than that, but still sufficient for 8 pins of servo 
        //  PWM.
        cmd[n++] = 0xff;
        cmd[n++] = 0xff; // 256
        cmd[n++] = 0xff;
        cmd[n++] = 0xff; // 512
        cmd[n++] = 0xfe;
        cmd[n++] = 0xff; // 768
        cmd[n++] = 0xfc;
        cmd[n++] = 0xff; // 1024
        cmd[n++] = 0xf8;
        cmd[n++] = 0xff; // 1280
        cmd[n++] = 0xf0;
        cmd[n++] = 0xff; // 1536
        cmd[n++] = 0xe0;
        cmd[n++] = 0xff; // 1792
        cmd[n++] = 0xc0;
        cmd[n++] = 0xff; // 2048
        cmd[n++] = 0x80;
        cmd[n++] = 0xff; // 2304
        cmd[n++] = 0;
        cmd[n++] = 0;    //  leave it at 0

        i = libusb_bulk_transfer(dh, DATA_OUT_EPNUM, cmd, n, &x, 1000);
        if (i != 0) {
            fprintf(stderr, "Could not send wave: %d\n", i);
            return 4;
        }
        if (x != n) {
            fprintf(stderr, "Short write (%d/%d) on wave: %d\n", x, n, i);
            return 5;
        }

        //  run at close to 100 Hz
        usleep(10000);
    }

    /*NOTREACHED*/
    return 0;
}

