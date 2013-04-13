
#define F_CPU 20000000

#include "libavr.h"
#include <avr/pgmspace.h>

#define XBEE_BD 7           //  115200 baud
#define XBEE_ID 6219
#define XBEE_RECV 20
#define XBEE_SEND 18
#define XBEE_DL XBEE_SEND
#define XBEE_MY XBEE_RECV

#define STRINGIZE2(x) #x
#define STRINGIZE(x) STRINGIZE2(x)
#define XBEE_SETUP "ATMY" STRINGIZE(XBEE_MY) " DL" STRINGIZE(XBEE_DL) " ID" STRINGIZE(XBEE_ID)

#define NODE_XBEE 1
#define NODE_RS485 2

//  port B
#define RESET_XBEE (1 << 0)
#define MOSI (1 << 3)
#define MISO (1 << 4)
#define CLOCK (1 << 5)

//  port D
#define RXD (1 << 0)
#define TXD (1 << 1)

unsigned short counterValues[6];
unsigned char triggerFlags;


TWIMaster *twi;

class MyMaster : public ITWIMaster {
public:
    void data_from_slave(unsigned char n, void const *data) {
    }
    void xmit_complete() {
    }
    void nack() {
        //  how can this happen?
    }
};
MyMaster master;

unsigned char packetseq;
unsigned char databuf[32];
unsigned char dataptr;


void do_packet(unsigned char sz, unsigned char const *data) {
    if (!twi->is_busy()) {
        twi->send_to(sz, data, NODE_RS485);
    }
    else {
        //  how can this happen?
    }
}

void recv_data(void *) {
    after(10, &recv_data, 0);
    unsigned char n = uart_read(sizeof(databuf) - dataptr, databuf);
try_another:
    //  if dataptr == 0, we're looking for packet start
    if (dataptr == 0) {
        for (unsigned char ch = 0; ch != n; ++ch) {
            if (databuf[ch] == 0xA0) {
                if (ch > 0) {
                    memmove(databuf, &databuf[ch], n - ch);
                }
                dataptr = n - ch;
                goto try_another;
            }
        }
        //  no start byte here
    }
    //  else, looking for complete packet, starting with length
    else {
        dataptr += n;
        if (dataptr >= 3) {
            if ((size_t)databuf[2] + 3 > sizeof(databuf)) {
                //  no chance of receiving this packet
                dataptr = 0;
            }
            else if (databuf[2] + 3 <= dataptr) {
                //  handle incoming packet
                do_packet(databuf[2], &databuf[3]);
                //  ack the sequence number
                char ack[2];
                ack[0] = 0xB0;
                ack[1] = databuf[1];
                uart_send_all(2, ack);
                //  look for more, if possible
                unsigned char left = dataptr - databuf[2] - 3;
                if (left > 0) {
                    memmove(databuf, &databuf[dataptr - left], left);
                    dataptr = 0;
                    n = left;
                    goto try_another;
                }
                dataptr = 0;
            }
        }
    }
}

unsigned char xbee_state;
unsigned char xbee_waits;
unsigned char xbee_okstate;

char const PROGMEM PLUSSES[] = "+++";
char const PROGMEM ATBDx[] = "ATBD" STRINGIZE(XBEE_BD) "\r";
char const PROGMEM SETUP[] = XBEE_SETUP "\r";
char const PROGMEM ATCN[] = "ATCN\r";

void setup_xbee(void *) {
    after(10, &setup_xbee, 0);
    //  discard data
    unsigned char dbrd = uart_read(sizeof(databuf), databuf);
    switch (xbee_state) {
    case 0:
        if (xbee_waits < 110) {
            ++xbee_waits;
        }
        else {
            ++xbee_state;
        }
        break;
    case 1:
        uart_send_all(3, strcpy_P((char *)databuf, PLUSSES));
        xbee_waits = 0;
        ++xbee_state;
        break;
    case 2:
        if (xbee_waits < 110) {
            ++xbee_waits;
        }
        else {
            ++xbee_state;
        }
        break;
    case 3:
        uart_send_all(6, strcpy_P((char *)databuf, ATBDx));
        xbee_waits = 0;
        ++xbee_state;
        break;
    case 4:
        if (xbee_waits < 110) {
            ++xbee_waits;
        }
        else {
            uart_setup(115200);
            xbee_waits = 0;
            ++xbee_state;
        }
        break;
    case 5:
        if (xbee_waits < 110) {
            ++xbee_waits;
        }
        else {
            ++xbee_state;
        }
        break;
    case 6:
        uart_send_all(3, strcpy_P((char *)databuf, PLUSSES));
        xbee_waits = 0;
        ++xbee_state;
        break;
    case 7:
        if (xbee_waits < 110) {
            ++xbee_waits;
        }
        else {
            ++xbee_state;
        }
        break;
    case 8:
        strcpy_P((char *)databuf, SETUP);
        uart_send_all(strlen((char *)databuf), databuf);
        xbee_waits = 0;
        xbee_okstate = 0;
        ++xbee_state;
        break;
    case 9:
        for (unsigned char i = 0; i < dbrd; ++i) {
            //  getting some data
            xbee_waits = 0;
            if (xbee_okstate == 0) {
                if (databuf[i] == 'O') {
                    xbee_okstate = 1;
                }
            }
            else if (xbee_okstate == 1) {
                if (databuf[i] == 'K') {
                    xbee_okstate = 2;
                }
                else {
                    xbee_okstate = 0;
                }
            }
            else if (xbee_okstate == 2) {
                if (databuf[i] == 13) {
                    //  success!
                    ++xbee_state;
                }
            }
        }
        ++xbee_waits;
        if (xbee_waits == 255) {
            //  timeout! crash and burn
            fatal(FATAL_UI_BAD_PARAM);
        }
        break;
    case 10:
        uart_send_all(5, strcpy_P((char *)databuf, ATCN));
        xbee_waits = 0;
        ++xbee_state;
        break;
    case 11:
        if (xbee_waits < 10) {
            ++xbee_waits;
        }
        else {
            ++xbee_state;
        }
        break;
    case 12:
        //  super success!
        ++xbee_state;
        after(10, &recv_data, 0);
        break;
    case 13:
        //  do nothing -- last enqueue
        break;
    }
}

void setup() {
    setup_timers(F_CPU);
    DDRB = RESET_XBEE | MISO | CLOCK;
    PORTB = 0;
    DDRD = TXD;
    delay(50);
    uart_setup(9600);
    PORTB = RESET_XBEE;
    twi = start_twi_master(&master);
    after(10, &setup_xbee, 0);
}

