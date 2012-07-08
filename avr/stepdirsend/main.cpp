#define F_CPU 8000000

#include <libavr.h>
#include <pins_avr.h>


#define BAUDRATE 57600
#define DIR_PORT PINC
#define STEP_PORT PINB

#define DEBUG_PIN 0x10


void blink(bool on) {
    DDRD |= 0xc0;
    if (on) {
        PORTD |= 0xc0;
    }
    else {
        PORTD &= ~0xc0;
    }
}

unsigned short counts[6];
unsigned char old_step = 0x00;

unsigned char buf[32];
unsigned char buf_ptr = 0;
unsigned short led_timeout;

void handle_packet() {
    if (buf[1] == 1 && buf[2] == 'a') {
        led_timeout = read_timer_fast() + 120;
        PORTD |= 0x80;
    }
}

bool debugpin = false;

//  PORTC is step signals
void read_pinstate(void *) {
    if (debugpin) {
        PORTD &= ~DEBUG_PIN;
        debugpin = false;
    }
    else {
        PORTD |= DEBUG_PIN;
        debugpin = true;
    }
    after(0, read_pinstate, 0);
    return;
    unsigned char dir = DIR_PORT; //  step
    unsigned char step = STEP_PORT; //  direction
    unsigned char mask = 1;
    for (unsigned char bit = 0; bit < 6; ++bit) {
        //  if this pin transitioned from low to high
        if ((step & mask) && !(old_step & mask)) {
            if (dir & mask) {
                counts[bit]++;
            }
            else {
                counts[bit]--;
            }
        }
        mask = mask << 1;
    }
    old_step = step;
    if (uart_available()) {
        PORTD |= 0x80;
        unsigned char ch = uart_getch();
        if (buf_ptr == 0) {
            if (ch != 0xed) {
                //  not a good packet
            }
            else {
                buf[0] = ch;
                buf_ptr = 1;
            }
        }
        else if (buf_ptr == 1) {
            if (ch > 30) {
                //  bad packet length
                buf_ptr = 0;
            }
            else {
                buf[1] = ch;
                buf_ptr = 2;
            }
        }
        else {
            buf[buf_ptr++] = ch;
            if (buf_ptr >= buf[1] + 2) {
                handle_packet();
                buf_ptr = 0;
            }
        }
    }
}

unsigned short calc_cksum(unsigned short *val, unsigned char cnt) {
    unsigned short ret = 0xbad1;
    while (cnt > 0) {
        //  overflow arithmetic is fun!
        ret = (ret + *val) * 0x6967 + 0x1551;
        --cnt;
        ++val;
    }
    return ret;
}


void send_update(void *p) {
    after(50, send_update, p);
    PORTD &= ~0x40;
    short diff = (short)(read_timer_fast() - led_timeout);
    if (diff > 0) {
        //  turn off yellow "ack" led
        PORTD &= ~0x80;
    }
    unsigned char sbuf[16] = { 0xed, 14 };
    memcpy(&sbuf[2], counts, 12);
    unsigned short cksum = calc_cksum((unsigned short *)sbuf, 7);
    memcpy(&sbuf[14], &cksum, 2);
    uart_send_all(16, sbuf);
    PORTD |= 0x40;
}

void setup() {
    fatal_set_blink(blink);
    DDRD |= 0xc0 | DEBUG_PIN;
    PORTD |= 0xc0 | DEBUG_PIN;
    setup_timers(F_CPU);
    uart_setup(BAUDRATE);
    power_adc_enable();
    DDRB &= ~0x3f;  //  direction, input
    DDRC &= ~0x3f;  //  step, input
    PORTC |= 0x3f;  //  pull-up
    PORTB |= 0x3f;  //  pull-up
    MCUCR &= ~(1 << PUD);
    after(50, send_update, 0);
    after(0, read_pinstate, 0);
}

