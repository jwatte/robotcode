#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

int main(void) {
  DDRB = 0xff; /* set PBx to output */
  DDRC = 0xff; /* set PBx to output */
  DDRD = 0xff; /* set PBx to output */
  while(1) {
    PORTB = 0; /* LED off */
    PORTC = 0; /* LED off */
    PORTD = 0; /* LED off */
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    PORTB = 0xff; /* LED on */
    PORTC = 0xff; /* LED on */
    PORTD = 0xff; /* LED on */
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
    _delay_ms(10);
  }
  return 0;
}

