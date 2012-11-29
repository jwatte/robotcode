
//  This code is put into the public domain 2012 by Jon Watte.
//
//  ATTiny84A code to implement four quadrature sensor/counters for gear 
//  motors with sensors (such as the Pololu 37D motors.)
//  The output is SPI-like, 8 bytes, formed as four two-byte unsigned shorts, 
//  little-endian, with current counts for each of the four inputs (modulo 
//  65536.)
//
//  The quadrature sensor is interpreted as "step + direction" rather than four 
//  separate states, to simplify the code (see comment below.)
//
//  Step pins: PA0-3.
//  Direction pins: PB0-2, PA7
//
//  This leaves PB3 to serve as !RESET, and PBA4,5,6 to use for SPI. There is 
//  an additional trick, here: The MOSI line is also used as chip select. Thus, 
//  first bring this low from the master, wait 50 us, then start clocking in 
//  data, waiting 50 us after each byte received. Send 0xff to stop the read 
//  operation, typically as the eigth byte.

#include <avr/io.h>
#include <avr/power.h>
#include <string.h>

unsigned char sending = 0;
unsigned char tosend[8] = { 0 };
unsigned short counts[4] = { 0x1111, 0x2222, 0x3333, 0x4444 };

int main() {

    DDRA = (1 << PA5);
    USICR = (1 << USIWM0) | (1 << USICS1);
    unsigned char ppa = 0; 
    while (true) {
        unsigned char pa = PORTA;
        unsigned char pb = PORTB;

        if (sending) {
            if (USISR & (1 << USIOIF)) {
                if (USIBR == 0xff) {
                    sending = 0;
                }
                else {
                    USIDR = tosend[sending & 7];
                    ++sending;
                }
                USISR = (1 << USIOIF);
            }
        }
        else if (!(pa & (1 << PA6))) {
            memcpy(tosend, counts, 8);
            USIDR = tosend[0];
            sending = 1;
        }

        //  Only look at low-to-high transitions.
        //  With 64 cpr, and 18.75:1 ratio, and only reading 1/4 quadrature changes, 
        //  this gives exactly 300 changes per output revolution. With 170mm wheels, 
        //  this gives a resolution of about 1.8mm per pulse. with more complex 
        //  code, I could decode each quadrature change for < 0.5mm precision, but 
        //  that would be ridiculous :-)
        //  With 300 changes per output revolution and 500 rpm, this already gives 
        //  2500 changes per second, which is a nice rate that can easily be sustained 
        //  without missing any pulses running at 8 Mhz.
        unsigned char xa = (pa ^ ppa) & pa; // what changed and is high?
        ppa = pa;
        if (xa & 1) {
            if (pb & 1) {
                counts[0]++;
            }
            else {
                counts[0]--;
            }
        }
        if (xa & 2) {
            if (pb & 2) {
                counts[1]++;
            }
            else {
                counts[1]--;
            }
        }
        if (xa & 4) {
            if (pb & 4) {
                counts[2]++;
            }
            else {
                counts[2]--;
            }
        }
        if (xa & 8) {
            if (pa & 128) {
                counts[3]++;
            }
            else {
                counts[3]--;
            }
        }
    }

    return 0;
}

