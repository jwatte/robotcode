#define F_CPU 8000000

#include <libavr.h>
#include <pins_avr.h>


#define BAUDRATE 57600
#define DIR_PORT PORTC
#define STEP_PORT PORTB

#define LED_BLUE 0x80
#define LED_YELLOW 0x40
#define LED_RED 0x20
#define LED_ALL 0xE0

#define DEBUG_PIN 0x10
#define IOBIT_MASK 0x0C



void blink(bool on) {
    DDRD |= LED_ALL;
    if (on) {
        PORTD |= LED_ALL;
    }
    else {
        PORTD &= ~LED_ALL;
    }
}


//  PORTC is step signals
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

unsigned char buf[32];
unsigned char recv_end = 0;
unsigned char recv_ptr = 0;
unsigned char steps = 0;
unsigned char directions = 0;

unsigned short counts[6];
unsigned short targets[6];
unsigned short steprates[6];
unsigned short stepphases[6];
unsigned short iobits = 0;

unsigned long blue_timeout;
bool debugpin;
bool first = true;

void toggle_debug() {
    debugpin = !debugpin;
    if (debugpin) {
        PORTD |= DEBUG_PIN;
    }
    else {
        PORTD &= ~DEBUG_PIN;
    }
}


unsigned char nak_packet[3] = {
    0xed,
    1,
    'n'
};

unsigned char ack_packet[3] = {
    0xed,
    1,
    'a'
};

void decode_packet() {
    unsigned short cs, ccs, newbits;
    if (buf[1] == 1 && buf[2] == 'r') {
        //  reset from board
        first = true;
        PORTD |= LED_ALL;
        return;
    }
    if (buf[1] != 16) {
        goto bad_packet;
    }
    memcpy(&cs, &buf[16], 2);
    ccs = calc_cksum((unsigned short *)buf, 8);
    if (cs != ccs) {
        goto bad_packet;
    }
    memcpy(targets, &buf[2], 12);
    memcpy(&newbits, &buf[14], 2);
    if (newbits != iobits) {
        iobits = newbits;
        PORTD |= (iobits & IOBIT_MASK) | LED_RED;
        PORTD &= (iobits | ~IOBIT_MASK);
    }
    if (first) {
        first = false;
        memcpy(counts, targets, 12);
    }
    for (unsigned char i = 0; i != 6; ++i) {
        short diff = (short)(targets[i] - counts[i]);
        if (diff < -10000) {
            diff = 10000;
        }
        else if (diff > 10000) {
            diff = 10000;
        }
        else if (diff < 0) {
            diff = -diff;
        }
        if (diff < 2) {
            diff = 2;
        }
        steprates[i] = 25000 / diff;
    }
    //  no longer blue, no longer lost packet
    PORTD &= ~LED_BLUE;
    blue_timeout = 0;
    uart_send_all(3, ack_packet);
    return;

bad_packet:
    //  bad packets make me feel blue
    PORTD |= LED_BLUE;
    wdt_reset();
    unsigned short max = 0;
    uart_send_all(3, nak_packet);
    while (!uart_available()) {
        udelay(10);
        if (max++ == 20) {
            //  no byte is there
            return;
        }
    }
    //  deliberately de-sync, because I might be treating 
    //  data as sync bytes
    uart_getch();
}

unsigned short prev = 0;

void main_loop(void *) {
    toggle_debug();
    if (uart_available()) {
        PORTD |= LED_YELLOW;
        unsigned char ch = (unsigned char)uart_getch();
        if (recv_ptr == 0) {
            if (ch != 0xed) {
                //  not a sync byte
            }
            else {
                buf[0] = ch;
                recv_ptr = 1;
            }
        }
        else if (recv_ptr == 1) {
            if (ch > sizeof(buf) - 2) {
                //  not a proper packet
                recv_ptr = 0;
                recv_end = 0;
            }
            else {
                buf[1] = ch;
                recv_end = 2 + ch;
                recv_ptr = 2;
            }
        }
        else {
            buf[recv_ptr] = ch;
            ++recv_ptr;
            if (recv_ptr == recv_end) {
                decode_packet();
                recv_ptr = 0;
                recv_end = 0;
            }
        }
        PORTD &= ~LED_YELLOW;
    }
    unsigned short now = uread_timer();
    unsigned short delta = now - prev;
    prev = now;
    //  if this loop takes 10 milliseconds, we're already in trouble...
    if (delta > 10000) {
        delta = 10000;
    }
    //uart_force_out(((unsigned char *)&delta)[0]);
    //uart_force_out(((unsigned char *)&delta)[1]);
    unsigned char mask = 1;
    bool change = false;
    for (unsigned char i = 0; i != 6; ++i) {
        if (targets[i] != counts[i]) {
            stepphases[i] += delta;
            if ((steps & mask) || (stepphases[i] >= steprates[i])) {
                change = true;
                if (!(steps & mask)) {
                    stepphases[i] -= steprates[i];
                }
                //  avoid too much accumulation of phase -- this 
                //  means a limit on slew rate
                if (stepphases[i] > 40000) {
                    stepphases[i] = 40000;
                }
                steps = steps ^ mask;
                if ((short)(targets[i] - counts[i]) > 0) {
                    directions |= mask;
                    if (!(steps & mask)) {
                        counts[i]++;
                    }
                }
                else {
                    directions &= ~mask;
                    if (!(steps & mask)) {
                        counts[i]--;
                    }
                }
            }
        }
        mask = mask << 1;
    }
    DIR_PORT = directions;
    if (change) {
        PORTD |= LED_RED;
    }
    else {
        PORTD &= ~LED_RED;
    }
    blue_timeout += delta;
    if (blue_timeout > 2000000) {
        PORTD |= LED_BLUE;
        first = true;
    }
    after(0, main_loop, 0);
    STEP_PORT = steps;
}


unsigned char online_packet[3] = { 
    0xed,
    1,
    'O'
};

void setup() {
    fatal_set_blink(blink);
    DDRD |= LED_ALL | DEBUG_PIN | IOBIT_MASK;
    PORTD = LED_ALL;
    setup_timers(F_CPU);
    uart_setup(BAUDRATE);
    uart_send_all(3, online_packet);
    power_adc_enable();
    DDRB |= 0x3f;  //  direction, output
    DDRC |= 0x3f;  //  step, output
    PORTC &= ~0x3f;  //  Start low
    PORTB &= ~0x3f;  //  Start low
    MCUCR &= ~(1 << PUD);
    after(0, main_loop, 0);
    //  turn off red and yellow
    PORTD &= ~(LED_RED | LED_YELLOW);
}

