#define F_CPU 16000000

#include "libavr.h"
#include "pins_avr.h"


void setup() {
    DDRD |= (1 << 4);
    PORTD &= ~(1 << 4);
    setup_timers(F_CPU);
    uart_setup(115200, F_CPU);
    unsigned char ch = 0;
    while (true) {
        uart_force_out(ch);
        ++ch;
        if (ch == 0) {
            PORTD |= (1 << 4);
            udelay(1000);
            PORTD &= ~(1 << 4);
        }
    }
}
