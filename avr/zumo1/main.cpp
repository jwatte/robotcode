
#define F_CPU 16000000

#include <libavr.h>
#include <pins_avr.h>
#include <avr/io.h>

unsigned char x = 0xff;
bool on_boop = false;

void boop_on() {
    if (!on_boop) {
        on_boop = true;
        TCCR2A = (1 << COM2B0) | (1 << WGM21);
        TCCR2B = (1 << CS22) | (1 << CS20);  //  clk / 128
        OCR2A = 0xFF;
        OCR2B = 0x0;
    }
}

void boop_off() {
    TCCR2A = 0;
    TCCR2B = 0;
    on_boop = false;
}


void motors_start() {
    TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
}

void motors_stop() {
    TCCR1B = (1 << WGM12);
}

void motors_power(int left, int right) {
    if (left < 0) {
        PORTD |= 0x80;
        left = -left;
    }
    else {
        PORTD &= ~0x80;
    }
    if (left > 255) {
        left = 255;
    }
    if (right < 0) {
        PORTB |= 0x1;
        right = -right;
    }
    else {
        PORTB &= ~0x1;
    }
    if (right > 255) {
        right = 255;
    }
    OCR1AH = 0;
    OCR1AL = left;
    OCR1BH = 0;
    OCR1BL = right;
}

bool on_nav = false;

void nav_on() {
    if (!on_nav) {
        on_nav = true;
        PORTB |= (1 << 5);
        motors_power(0, 0);
        motors_start();
    }
}

void nav_off() {
    PORTB &= ~(1 << 5);
    on_nav = false;
    motors_stop();
}

void blink(void *p)
{
    if (on_boop) {
        OCR2A = x;
        OCR2B = (x >> 1) + 1;
        x -= 1;
        if (x < 32) {
            x = 0xff;
            boop_off();
            nav_on();
        }
    }
    else if (on_nav) {
        motors_power(255, -255);
        if (!(PINB & (1 << 4))) {
            nav_off();
        }
    }
    else {
        if (!(PINB & (1 << 4))) {
            boop_on();
        }
    }
}

void setup() {
    //  I can't use timers!
    //  So I can't use "after"!
    //setup_timers(F_CPU);
    //  buzzer
    PORTB = (1 << 4);   //  pull-up
    DDRB = (1 << 5) | (1 << 2) | (1 << 1) | (1 << 0);
    PORTD = 0;
    DDRD = (1 << 3) | (1 << 7);

    power_timer2_enable();
    ASSR = 0;

    power_timer1_enable();
    TCCR1A = (1 << COM1A1) | (1 << COM1B1) | (1 << WGM10);
    TCCR1B = (1 << WGM12);
    TCNT1H = 0;
    TCNT1L = 0;
    OCR1AH = 0;
    OCR1AL = 0;
    OCR1BH = 0;
    OCR1BL = 0;


    while (true) {
        blink(0);
    }
}
