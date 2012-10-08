
#define F_CPU 16000000

#include <libavr.h>
#include <lcd.h>
#include <stdio.h>

#if HAS_UTFT

extern unsigned char const Droid_Sans_10_ascii_data[] PROGMEM;
Font TheFont(Droid_Sans_10_ascii_data);

bool lit = false;

void floop(void *n) {
    if (lit) {
        PORTB |= (1 << 5);
    }
    else {
        PORTB &= ~(1 << 5);
    }
    lit = !lit;

    char txt[32];
    sprintf(txt, "Value %d        ", (int)n);
    LCD::text(5, 5, 0x0000, 0xffff, txt, strlen(txt), TheFont);

    after(0, floop, (char *)n + 1);
}

void setup() {
    setup_timers(F_CPU);
    //  5 is blinky light
    //  3 is reset
    DDRB |= (1 << 5) + (1 << 3);
    PORTB |= (1 << 5) + (1 << 3);
    udelay(100);
    PORTB &= ~(1 << 3);
    udelay(100);
    PORTB |= (1 << 3);
    LCD::init();
    after(500, floop, 0);
    PORTB &= ~(1 << 5);
    LCD::clear(0xffff);
}

#endif

