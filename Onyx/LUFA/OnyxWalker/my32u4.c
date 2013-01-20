
#include "Config.h"
#include <avr/io.h>
#include <util/atomic.h>
#include <string.h>
#include <avr/power.h>
#include "my32u4.h"


static volatile unsigned char rbuf[128];
static volatile unsigned char rptr;
static volatile unsigned char nmissed;

ISR(USART1_RX_vect) {
    while ((UCSR1A & (1 << RXC1)) != 0) {
        unsigned char d = UDR1;
        if (rptr < sizeof(rbuf)) {
            rbuf[rptr] = d;
            ++rptr;
        }
        else {
            ++nmissed;
            PORTB |= BLUE_LED;
        }
    }
}

void setup_uart(unsigned char rate) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        power_usart1_enable();
        PORTD |= (1 << PD3) | (1 << PD2);   //  TXD, RXD pull-up
        DDRD &= ~((1 << PD2) | (1 << PD3));//  RXD
        rptr = 0;
        nmissed = 240;
        UCSR1C = (1 << USBS1) | (1 << UCSZ11) | (1 << UCSZ10);   //  async mode, 8N2
        UCSR1A = (1 << U2X1);       //  double speed (also, more accurate!)
        UCSR1B = (1 << RXCIE1) | (1 << RXEN1) | (1 << TXEN1);
        switch (rate) {
            case BRATE_9600:
                UBRR1 = 207;
                break;
            case BRATE_57600:
                UBRR1 = 34;
                break;
            case BRATE_1000000:
                UBRR1 = 1;
                break;
            case BRATE_2000000:
                UBRR1 = 0;
                break;
            default:
                // 'rate' is a divider value
                UBRR1 = rate;
                break;
        }
        //  a small delay
        while (nmissed) {
            ++nmissed;
        }
    }
}

void send_sync(unsigned char const *data, unsigned char size) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        //  enable transmitter
        UCSR1B = (1 << TXEN1);
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
        //  disable transmitter, enable interrupt
        UCSR1B = (1 << RXCIE1) | (1 << RXEN1);
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

void delayus(unsigned short us) {
    if (us > 500) {
        us = 500;
    }
    unsigned short delayed = 0;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        unsigned char tc = TCNT0;
        while (delayed < us) {
            unsigned char tc2 = TCNT0;
            delayed += (tc2 - tc) * 5;
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
    while (1) {
        PORTB |= BLUE_LED;
        delayms(50);
        PORTB &= ~0xf;
        delayms(50);
        PORTB = (PORTB & 0xf0) | (errkind & 0xf);
        delayms(200);
        PORTB &= ~0xf;
        delayms(20);
        unsigned char buf[8] = {
            0xee, 0xee, errkind, errdata, 
            epic, epiir, epirwa,
            sbuflen
        };
        send_sync(buf, 8);
        send_sync(sbuf, sbuflen);
    }
}

