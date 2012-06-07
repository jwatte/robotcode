
#include <gps.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

gps_data_t gps;

int main() {
    int err = 0;
    if ((err = gps_open("localhost", "2947", &gps)) < 0) {
        fprintf(stderr, "gps_open(): %s\n", gps_errstr(err));
        exit(1);
    }
    if ((err = gps_stream(&gps, WATCH_NEWSTYLE | WATCH_SCALED, 0)) < 0) {
        fprintf(stderr, "gps_stream(): %s\n", gps_errstr(err));
        exit(1);
    }
    while (true) {
        if (gps_waiting(&gps, 500)) {
            errno = 0;
            if (gps_read(&gps) == -1) {
                perror("gps_read()");
                exit(1);
            }
            if (gps.set & ONLINE_SET) {
                fprintf(stdout, "ONLINE=%f ", gps.online);
            }
            if (gps.set & TIME_SET) {
                fprintf(stdout, "TIME=%f ", gps.fix.time);
            }
            if (gps.set & LATLON_SET) {
                fprintf(stdout, "LAT/LON=%f/%f ", gps.fix.latitude, gps.fix.longitude);
            }
            fprintf(stdout, "STATUS=%d ", gps.status);
            if (gps.status) {
                fprintf(stdout, "NSAT=%d ", gps.satellites_visible);
            }
            fprintf(stdout, "\n");
            fflush(stdout);
            gps.set = 0;
        }
        usleep(100000);
    }
}
