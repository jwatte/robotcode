
#include "Config.h"
#include <avr/io.h>
#include <util/atomic.h>
#include <string.h>
#include <avr/power.h>
#include "my32u4.h"


static volatile unsigned char rbuf[128];
static volatile unsigned char rptr;
static volatile unsigned char nmissed;

static volatile unsigned char status;
static volatile unsigned char status_override;
static volatile unsigned char status_override_mask;

void setup_status(void) {
    DDRF = DDRF | 0x80;
    DDRE = DDRE | 0x40 | 0x4;
    DDRD = DDRD | 0x80 | 0x40 | 0x2 | 0x1;
    DDRB = DDRB | 0x8;
}

void set_status(unsigned char value, unsigned char mask) {
    status = (status & ~mask) | (value & mask);
    unsigned char show_status = (status & ~status_override_mask) | status_override;
    PORTF = (PORTF & ~0x80) |
        ((show_status & 1) ? 0x80 : 0);
    PORTE = (PORTE & ~(0x40 | 0x4)) |
        ((show_status & 2) ? 0x40 : 0) |
        ((show_status & 4) ? 0x4 : 0);
    PORTD = (PORTD & ~(0x80 | 0x40 | 0x2 | 0x1)) |
        ((show_status & 8) ? 0x80 : 0) |
        ((show_status & 0x10) ? 0x40 : 0) |
        ((show_status & 0x20) ? 0x2 : 0) |
        ((show_status & 0x40) ? 0x1 : 0);
    PORTB = (PORTB & ~0x8) | ((show_status & 0x80) ? 0x8 : 0);
}

void set_status_override(unsigned char value, unsigned char mask) {
    status_override = value & mask;
    status_override_mask = mask;
    set_status(status, 0xff);
}




volatile unsigned short timer = 0;

ISR(TIMER0_COMPB_vect) {
    timer += 1;
}

unsigned short getms(void) {
    unsigned short ret;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ret = timer;
    }
    return ret;
}

void delayms(unsigned short ms) {
    if (ms > 10000) {
        //  maximum delay -- 10 seconds
        ms = 10000;
    }
    if ((SREG & 0x80) == 0) {
        //  disabled interrupts delay
        unsigned char old = TCNT0;
        while (ms > 0) {
            unsigned char cur = TCNT0;
            if (cur < old) {    //  a wrap detected
                --ms;
                ++timer;
            }
            old = cur;
        }
    }
    else {
        unsigned short cur, start;
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            start = timer;
        }
        while (1) {
            ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
                cur = timer;
            }
            if (cur - start >= ms) {
                break;
            }
            //  do nothing
        }
    }
}

/* note: this keeps interrupts disabled! */
void delayus(unsigned short us) {
    if (us > 500) {
        us = 500;
    }
    unsigned short delayed = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        unsigned char tc = TCNT0;
        while (delayed < us) {
            unsigned char tc2 = TCNT0;
            //  each time we wrap around 250, we get another 20 microseconds
            //  that we didn't actually spend...
            if (tc2 < tc) {
                tc += 5;
            }
            delayed += (unsigned short)((unsigned char)(tc2 - tc)) * 4;
            tc = tc2;
        }
    }
}

void setup_delay(void) {
    power_timer0_enable();
    TCNT0 = 0;
    OCR0A = 250;    //  exactly a millisecond per interrupt
    OCR0B = 0;
    TIMSK0 = (1 << OCIE0B);
    TCCR0A = (1 << WGM01);  //  CTC mode
    TCCR0B = (1 << CS01) | (1 << CS00); //  Clock / 64 == 250000 pulses per second
}

extern unsigned char epic;
extern unsigned char epiir;
extern unsigned char epirwa;


void show_error(unsigned char errkind, unsigned char errdata) {

    cli();
    set_status_override(0, 0);
    while (1) {
        set_status(0xff, 0xff);
        delayms(50);
        set_status(0, 0xff);
        delayms(100);
        set_status(errkind, 0xff);
        delayms(400);
        set_status(0, 0xff);
        delayms(100);
        set_status(errdata, 0xff);
        delayms(400);
        set_status(0, 0xff);
        /*
        unsigned char buf[8] = {
            0xee, 0xee, errkind, errdata, 
            epic, epiir, epirwa,
            sbuflen
        };
        send_sync(buf, 8, 0);
        send_sync(sbuf, sbuflen, 0);
        */
        delayms(100);
    }
}

void setup_adc(void) {
    power_adc_enable();
    ADMUX = (1 << REFS0)    //  AVcc
        | 6                 //  channel 6 (PF6, where the battery is)
        ;
    ADCSRA = (1 << ADEN)    //  enable
        | (1 << ADIF)       //  clear complete flag
        | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0)    //  clock divide by 128
        ;
    ADCSRB = 0;
    DIDR0 = (1 << ADC6D)    //  disable digital buffer pin F6
        ;
    DIDR2 = 0;
    ADCSRA |= (1 << ADSC);  //  read once, to bootstrap the system
}


