
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

//  port B
#define CTR_POWER (1 << 2)
#define TRIGGER_IN (1 << 1)
#define RESET_XBEE (1 << 0)
#define MOSI (1 << 3)
#define MISO (1 << 4)
#define CLOCK (1 << 5)

//  port C
#define ALL_CS 0x3f
#define CS(x) (1 << (x))

//  port D
#define RXD (1 << 0)
#define TXD (1 << 1)
#define ALL_DEBUG 0xfc
#define DEBUG(x) (1 << ((x) + 2))

unsigned short counterValues[6];
unsigned char triggerFlags;

void enable_tinys() {
    PORTC &= ALL_CS;
    PORTB |= CTR_POWER;
    delay(50);
}

void read_ctr(void *c) {
    unsigned char ctr = (unsigned char)(short)c;

    PORTC &= ~ALL_CS;
    PORTC |= CS(ctr);
    //  clock out two bytes, reading status big-endian
    udelay(20);
    SPDR = 0;
    unsigned short value;
    while (!(SPSR & (1 << SPIF))) {
        //  wait
    }
    value = ((unsigned short)SPDR << 8);
    udelay(20);
    SPDR = 0;
    while (!(SPSR & (1 << SPIF))) {
        //  wait
    }
    value |= (unsigned short)SPDR;
    PORTC &= CS(ctr);

    counterValues[ctr] = value;
    //  todo: send to xbee

    ctr = ctr + 1;
    if (ctr == 6) {
        ctr = 0;
    }
    after(0, &read_ctr, (void *)(short)ctr);
}

unsigned char packetseq;
unsigned char databuf[32];

void send_data(void *) {
    after(30, &send_data, 0);
    databuf[0] = 0xA0;      //  command byte
    databuf[1] = packetseq;
    ++packetseq;
    databuf[2] = 6 * 2 + 1; //  six counters, one flags
    databuf[3] = triggerFlags;
    memcpy(&databuf[4], counterValues, 6 * 2);
    //  This should always succeed anyway, because the 
    //  data rate is lower than the baud rate by a lot, to 
    //  compensate for lossy xbees.
    uart_send_all(16, databuf);
}

unsigned char xbee_state;
unsigned char xbee_waits;
unsigned char xbee_okstate;

char const PROGMEM PLUSSES[] = "+++";
char const PROGMEM ATBD7[] = "ATBD7\r";
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
        uart_send_all(6, strcpy_P((char *)databuf, ATBD7));
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
        after(30, &send_data, 0);
        break;
    case 13:
        //  do nothing -- last enqueue
        break;
    }
}

void setup() {
    setup_timers(F_CPU);
    DDRB = RESET_XBEE | MISO | CLOCK | CTR_POWER;
    PORTB = 0;
    DDRC = ALL_CS;
    DDRD = TXD;
    delay(50);
    uart_setup(9600);
    PORTB = 1;
    enable_tinys();
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
    after(0, &read_ctr, 0);
    after(10, &setup_xbee, 0);
}

