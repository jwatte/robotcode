#define F_CPU 8000000
#include <libavr.h>
#include <pins_avr.h>

void blink(void *p)
{
    if (p) {
        PORTD |= 0xc0;
        PORTB |= (1 << 5);
    }
    else {
        PORTD &= ~0xc0;
        PORTB &= ~(1 << 5);
    }
    after(200, blink, p ? NULL : (void *)1);
}

void setup() {
    setup_timers(F_CPU);
    DDRD |= 0x0c0;
    DDRB |= (1 << 5);
    after(200, blink, 0);
}
