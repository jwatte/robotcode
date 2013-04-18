#define F_CPU 20000000
#include <libavr.h>
#include <pins_avr.h>

void blink(void *p)
{
    if (p) {
        PORTD |= 0x04;
    }
    else {
        PORTD &= ~0x04;
    }
    after(200, blink, p ? NULL : (void *)1);
}

void setup() {
    setup_timers(F_CPU);
    DDRD |= 0x04;
    after(200, blink, 0);
}
