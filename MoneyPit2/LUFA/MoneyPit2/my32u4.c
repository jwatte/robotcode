
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

void setup_status() {
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


ISR(USART1_RX_vect) {
    while ((UCSR1A & (1 << RXC1)) != 0) {
        unsigned char d = UDR1;
        if (rptr < sizeof(rbuf)) {
            rbuf[rptr] = d;
            ++rptr;
        }
        else {
            ++nmissed;
        }
    }
}

static unsigned char old_ubrr = 0xff;

void setup_uart(unsigned char rate) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        power_usart1_enable();
        PORTD |= (1 << PD3) | (1 << PD2);   //  TXD, RXD pull-up
        DDRD &= ~((1 << PD2) | (1 << PD3));//  RXD
        rptr = 0;
        UCSR1C = (1 << USBS1) | (1 << UCSZ11) | (1 << UCSZ10);   //  async mode, 8N2
        UCSR1A = (1 << U2X1);       //  double speed (also, more accurate!)
        unsigned char new_ubrr = rate;
        switch (rate) {
            case BRATE_9600:
                new_ubrr = 207;
                break;
            case BRATE_57600:
                new_ubrr = 34;
                break;
            case BRATE_1000000:
                new_ubrr = 1;
                break;
            case BRATE_2000000:
                new_ubrr = 0;
                break;
            default:
                // 'rate' is a divider value
                new_ubrr = rate;
                break;
        }
        if (old_ubrr != new_ubrr || old_ubrr == 0xff) {
            UBRR1H = 0;
            UBRR1L = new_ubrr;
            UCSR1B = (1 << RXCIE1) | (1 << RXEN1) | (1 << TXEN1);   //  transmitter and receiver enable
            old_ubrr = new_ubrr;
            delayms(100);
        }
        nmissed = 0;
    }
}

extern unsigned short clear_received;   //  ugh! hack!

void send_sync(unsigned char const *data, unsigned char size, unsigned char enable_rx_intr) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        //  enable transmitter, disable receiver and interrupt
        UCSR1B = (1 << TXEN1);
        clear_received = getms() + 10;
        set_status(SENDING_LED, SENDING_LED);
        //  send data
        while (size > 0) {
            while ((UCSR1A & (1 << UDRE1)) == 0) {
                //  wait for send to drain
            }
            UDR1 = *data;
            UCSR1A |= (1 << TXC1);
            ++data;
            --size;
        }
        //  wait for last byte to be pushed out
        while ((UCSR1A & (1 << TXC1)) == 0) {
            //  wait for transmit to complete
        }
        rptr = 0;
        //  enable interrupt
        UCSR1B = (enable_rx_intr ? (1 << RXCIE1) : 0) | (1 << RXEN1) | (1 << TXEN1);
    }
}

unsigned char recv_avail(void) {
    return rptr;
}

unsigned char const *recv_buf(void) {
    return (unsigned char const *)(rbuf);
}

void recv_eat(unsigned char cnt) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (cnt >= rptr) {
            rptr = 0;
        }
        else {
            unsigned char left = rptr - cnt;
            memmove((unsigned char *)rbuf, (unsigned char const *)&rbuf[cnt], left);
            rptr = left;
        }
    }
}

unsigned char get_nmissed(void) {
    unsigned char ret = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        ret = nmissed;
        nmissed = 0;
    }
    return ret;
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

extern unsigned char sbuf[];
extern unsigned char sbuflen;
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
        delayms(50);
        set_status(errkind, 0xff);
        delayms(300);
        set_status(0, 0xff);
        delayms(50);
        set_status(errdata, 0xff);
        delayms(300);
        unsigned char buf[8] = {
            0xee, 0xee, errkind, errdata, 
            epic, epiir, epirwa,
            sbuflen
        };
        send_sync(buf, 8, 0);
        send_sync(sbuf, sbuflen, 0);
    }
}

