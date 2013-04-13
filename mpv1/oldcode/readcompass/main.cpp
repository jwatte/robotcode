
#define F_CPU 16000000L

#include "libavr.h"
#include <stdio.h>

#define ACCEL 0x18
#define MAG 0x1E


void read_mag(void *);

TWIMaster *master;
class MyMaster : public ITWIMaster {
    public:
        virtual void data_from_slave(unsigned char n, void const *data) {
            char buf[20];
            unsigned char const *d = (unsigned char const *)data;
                    //  less than 20 characters!
            sprintf(buf, "M %04x %04x %04x\r\n",
                    (d[0] << 8) | d[1], 
                    (d[2] << 8) | d[3],
                    (d[4] << 8) | d[5]);
            uart_send_all(strlen(buf), buf);
            after(250, read_mag, 0);
        }
        virtual void xmit_complete() {
            char buf[2];
            unsigned char next_state = state_ + 1;
            switch (state_) {
                case 0:
                    buf[0] = 0x00;     //  CRA_REG_M
                    buf[1] = 0x14;  //  30 Hz rate
                    master->send_to(2, buf, MAG);
                    break;
                case 1:
                    buf[0] = 0x02;  //  MR_REG_M
                    buf[1] = 0x00;  //  continuous conversion
                    master->send_to(2, buf, MAG);
                    break;
                case 2:
                    buf[0] = 0x3;
                    master->send_to(1, buf, MAG);
                    break;
                default:
                    //  register select -- now, do data from MAG
                    master->request_from(MAG, 6);
                    next_state = 2;
                    break;
            }
            state_ = next_state;
        }
        virtual void nack() {
            uart_send_all(5, "nak\r\n");
        }


        MyMaster() :
            state_(0) {
        }
        void boot() {
            xmit_complete();
        }
        unsigned char state_;
};
MyMaster myMaster;

void read_mag(void *) {
    myMaster.xmit_complete();
}

void write_status(void *p) {
    char buf[12];
    sprintf(buf, "ok %d\r\n", (int)(size_t)p);
    uart_send_all(strlen(buf), buf);
    after(5000, write_status, (char *)p + 1);
}

void boot_compass(void *) {
    myMaster.boot();
}

void setup() {
    setup_timers(F_CPU);
    uart_setup(115200);
    master = start_twi_master(&myMaster);
    after(100, boot_compass, 0);
    after(5000, write_status, 0);
    uart_send_all(6, "boot\r\n");
}
