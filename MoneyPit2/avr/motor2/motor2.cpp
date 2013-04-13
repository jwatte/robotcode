
#define F_CPU 20000000

#include <libavr.h>
#include <pins_avr.h>
#include <avr/io.h>

unsigned char lastread[8];

void read_spi(void *) {
    after(1, read_spi, 0);
    uint8_t idi = disable_interrupts();
    SPCR = (1 << SPE) | (1 << MSTR) | (1 << SPR0);
    for (unsigned char c = 0; c < 8; ++c) {
        SPDR = (c == 7 ? 0xff : 0);
        while (!(SPSR & (1 << SPIF))) {
            //  twiddle thumbs
        }
        lastread[c] = SPDR;
        if (c < 7) {
            //  Let interrupts be on while waiting for the next iteration.
            restore_interrupts(idi);
            //  The overhead in the loop, plus this small delay, makes for 
            //  sufficient delay to robustly use semi-software SPI. It seemed 
            //  stable at 5 us, so I set it to 6 for good measure. The actual 
            //  time between end-to-start is 40 microseconds here, which means 
            //  that the counter running at 25 kHz sampling is enough, and I 
            //  think it runs slightly faster than that when clocked at 8 MHz.
            //  The consideration is to not miss I2C interrupts or keep the 
            //  I2C bus in hold for too long if a command comes in while the 
            //  counter reading is taking place. Getting some extra time 
            //  between samples is safe, because the quadrature encoder takes 
            //  a snapshot of data when enable goes low, and then transfers 
            //  that out.
            udelay(6);
            idi = disable_interrupts();
        }
    }
    PORTB |= (1 << PB3);    //  MOSI as chip select, too
    SPCR = (1 << MSTR) | (1 << SPR0);
    restore_interrupts(idi);
    //  now what?
}

void setup() {
    PORTB = (1 << PB3);     //  MOSI as chip select, too
                            //  PB2 is reset
    DDRB = (1 << PB3) | (1 << PB5) | (1 << PB2);    //  PB2 must be an output to avoid losing master
    PORTC = 0;
    DDRC = (1 << 5);

    SPCR = (1 << MSTR) | (1 << SPR0);
    SPSR = 0;

    setup_timers(F_CPU);
    power_adc_enable();

    PORTB |= (1 << PB2);    //  take counter out of reset

    //  give counter time to start up
    after(100, read_spi, 0);
}

