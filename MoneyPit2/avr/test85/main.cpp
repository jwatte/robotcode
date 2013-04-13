
#include <avr/io.h>
#include <avr/wdt.h>


unsigned char prev_b = 0;
unsigned char flank = 0;
unsigned short val;

//  CHIPSELECT is active high
#define CHIPSELECT (1 << 0)
#define DATAOUT (1 << 1)
#define CLOCKIN (1 << 2)
#define STEPIN (1 << 3)
#define DIRIN (1 << 4)
#define DEBUGOUT (1 << 5)


int main() {
    DDRB |= DATAOUT | DEBUGOUT;
    PORTB = DATAOUT;
    while (true) {

        unsigned char b = PINB;

        if (b & CHIPSELECT) {
            if (!flank) {
                USICR = (1 << USIWM0) | (1 << USICS1);
                PORTB &= ~DATAOUT;
                USIDR = (val >> 8);
                USISR = (1 << USIOIF);
                flank = 1;
            }
            else if (flank == 1) {
                if (USISR & (1 << USIOIF)) {
                    USIDR = (val & 0xff);
                    USISR = (1 << USIOIF);
                    flank = 2;
                    val += 1;
                }
            }
        }
        else {
            if (flank == 2) {
                flank = 0;
                USICR = 0;
                PORTB |= DATAOUT;
            }
        }

        if (b & STEPIN) {
            if (!prev_b) {
                if (b & DIRIN) {
                    ++val;
                }
                else {
                    --val;
                }
                prev_b = 1;
            }
        }
        else {
            prev_b = 0;
        }
    }
}

