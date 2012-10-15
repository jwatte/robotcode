
#define F_CPU 20000000

#include "libavr.h"

//  port B
#define CTR_PWR (1 << 2)
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

void enable_tinys() {
    PORTC |= ALL_CS;
    PORTB |= CTR_PWR;
    delay(10);
}

void read_ctr(void *c) {
    unsigned char ctr = (unsigned char)(short)c;

    PORTC |= ALL_CS;
    PORTC &= ~(CS(ctr));
    //  clock out two bytes, reading status big-endian
    udelay(10);
    SPDR = 0;
    unsigned short value;
    while (!(SPSR & (1 << SPIF))) {
        //  wait
    }
    value = ((unsigned short)SPDR << 8);
    udelay(10);
    SPDR = 0;
    while (!(SPSR & (1 << SPIF))) {
        //  wait
    }
    value |= (unsigned short)SPDR;
    PORTC = ~(CS(ctr));

    counterValues[ctr] = value;
    //  todo: send to xbee

    ctr = ctr + 1;
    if (ctr == 6) {
        ctr = 0;
    }
    after(0, &read_ctr, (void *)(short)ctr);
}

void setup() {
    setup_timers(F_CPU);
    DDRB = RESET_XBEE | MISO | CLOCK;
    DDRC = ALL_CS;
    DDRD = TXD;
    enable_tinys();
    //  todo: setup xbee
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
    after(0, &read_ctr, 0);
}

