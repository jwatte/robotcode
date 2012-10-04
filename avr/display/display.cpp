
#define F_CPU 16000000

#include <libavr.h>
#include <lcd.h>


bool lit = false;

void floop(void *) {
    if (lit) {
        PORTB |= (1 << 5);
    }
    else {
        PORTB &= ~(1 << 5);
    }
    lit = !lit;

    LCD::clear(0xffff);
    LCD::clear(0x07ff);
    LCD::clear(0xf81f);
    LCD::clear(0xffe0);
    LCD::clear(0xf800);
    LCD::clear(0x07e0);
    LCD::clear(0x001f);
    LCD::clear();

    after(1000, floop, 0);
}

void setup() {
    setup_timers(F_CPU);
    DDRB |= (1 << 5) + (1 << 6);
    PORTB |= (1 << 5) + (1 << 6);
    udelay(100);
    PORTB &= ~(1 << 6);
    udelay(100);
    PORTB |= (1 << 6);
    LCD::init();
    after(1000, floop, 0);
    PORTB &= ~(1 << 5);
}

