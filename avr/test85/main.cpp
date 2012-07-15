
#include <avr/io.h>
#include <avr/wdt.h>


unsigned char prev_b = 0;
unsigned char flank = 0;
unsigned short val;
unsigned char flash = 0;

int main() {
    DDRB |= (1 << 4) | (1 << 0);    //  PB4, MOSI
    USICR = (1 << USIWM0) | (1 << USICS1);
    while (true) {
        unsigned char b = PINB;
        if (!(b & 2)) {
            if (!flank) {
                USIDR = (val >> 8);
                USISR = (1 << USIOIF);
                flank = 1;
            }
            else if (flank == 1) {
                if (USISR & (1 << USIOIF)) {
                    USIDR = (val & 0xff);
                    USISR = (1 << USIOIF);
                    flank = 2;
                }
            }
        }
        else {
            flank = 0;
        }
        if (b & (1 << 3)) {
            if (!prev_b) {
                if (b & (1 << 4)) {
                    ++val;
                }
                else {
                    --val;
                }
            }
        }
        else {
            prev_b = 0;
        }
        if (flash) {
            PORTB |= (1 << 4);
        }
        else {
            PORTB &= ~(1 << 4);
        }
        flash = 1-flash;
    }
}

