#if !defined(pins_avr_h)
#define pins_avr_h

/* I wish I didn't have to define those macros for pinMode, etc, but 
   doing so saves hundreds of bytes compared to using the functions :-( */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "libavr.h"

/*
   For emulating Wiring / Arduino, I map ports to pins:
   PORTB -- pins 0 through 7
   PORTC -- pins 8 through 15
   PORTD -- pins 16 through 23

   Additionally, the INPUT/OUTPUT and LOW/HIGH constants.
 */

enum {
    INPUT = 0, OUTPUT = 1,
    LOW = 0, HIGH = 1
};



inline void sbi(uint8_t volatile *reg, uint8_t val) {
    *reg |= val;
}

inline void cbi(uint8_t volatile *reg, uint8_t val) {
    *reg &= ~val;
}

#if defined(__AVR_ATtiny84A__)

#define pinToPortRegOut(pin) \
    (((pin & 0x18) == 0) ? PORTA : ((pin & 0x18) == 0x8) ? PORTB : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pinToPortRegIn(pin) \
    (((pin & 0x18) == 0) ? PINA : ((pin & 0x18) == 0x8) ? PINB : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pinToPortDirReg(pin) \
    (((pin & 0x18) == 0) ? DDRA : ((pin & 0x18) == 0x8) ? DDRB : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pcMaskReg(pin) \
    (((pin & 0x18) == 0) ? PCMSK0 : ((pin & 0x18) == 0x8) ? PCMSK1 :(fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pcCtlBit(pin) \
    (1 << (((pin & 0x18) == 0) ? PCIE0 : ((pin & 0x18) == 0x8) ? PCIE1 :(fatal(FATAL_BAD_PIN), *(regtype *)0)))

inline regtype &pcCtlReg(uint8_t pin) {
    return GIMSK;
}

#define HAS_UTFT 0
#define HAS_UART 0
#define HAS_SPI 0
#define NATIVE_TWI 0
#define HAVE_16BIT_TIMER1 1
#define ADC_DDR DDRA
#define ADC_PORT PORTA
#define NUM_PCINTS 2
#define PIN_PCINT0 PINA
#define PIN_PCINT1 PINB
#define TIMER1_OVF_vect TIM1_OVF_vect
#define TIMER1_COMPA_vect TIM1_COMPA_vect


#elif defined(__AVR_ATtiny85__)

#define pinToPortRegOut(pin) \
    (((pin & 0x18) == 0) ? PORTB : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pinToPortRegIn(pin) \
    (((pin & 0x18) == 0) ? PINB : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pinToPortDirReg(pin) \
    (((pin & 0x18) == 0) ? DDRB : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pcMaskReg(pin) \
    (((pin & 0x18) == 0) ? PCMSK : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pcCtlBit(pin) \
    (1 << (((pin & 0x18) == 0) ? PCIE :(fatal(FATAL_BAD_PIN), *(regtype *)0)))

inline regtype &pcCtlReg(uint8_t pin) {
    return GIMSK;
}

#define HAS_UTFT 0
#define HAS_UART 0
#define HAS_SPI 0
#define NATIVE_TWI 0
#define HAVE_16BIT_TIMER1 0
#define ADC_DDR DDRB
#define ADC_PORT PORTB
#define NUM_PCINTS 1
#define PIN_PCINT0 PINB


#elif defined(__AVR_ATmega328P__)

/* for hysterical reasons, Atmega328p starts with pin group == port b */
#define pinToPortRegOut(pin) \
    (((pin & 0x18) == 0) ? PORTB : ((pin & 0x18) == 0x8) ? PORTC : ((pin & 0x18) == 0x10) ? PORTD : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pinToPortRegIn(pin) \
    (((pin & 0x18) == 0) ? PINB : ((pin & 0x18) == 0x8) ? PINC : ((pin & 0x18) == 0x10) ? PIND : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pinToPortDirReg(pin) \
    (((pin & 0x18) == 0) ? DDRB : ((pin & 0x18) == 0x8) ? DDRC : ((pin & 0x18) == 0x10) ? DDRD : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pcMaskReg(pin) \
    (((pin & 0x18) == 0) ? PCMSK0 : ((pin & 0x18) == 0x8) ? PCMSK1 : ((pin & 0x18) == 0x10) ? PCMSK2 : (fatal(FATAL_BAD_PIN), *(regtype *)0))
#define pcCtlBit(pin) \
    (1 << (((pin & 0x18) == 0) ? PCIE0 : ((pin & 0x18) == 0x8) ? PCIE1 : ((pin & 0x18) == 0x10) ? PCIE2 : (fatal(FATAL_BAD_PIN), *(regtype *)0)))

inline regtype &pcCtlReg(uint8_t pin) {
    return PCICR;
}

#define HAS_UTFT 1
#define HAS_UART 1
#define HAS_SPI 1
#define NATIVE_TWI 1
#define HAVE_16BIT_TIMER1 1
#define ADC_DDR DDRC
#define ADC_PORT PORTC
#define NUM_PCINTS 3
#define PIN_PCINT0 PINB
#define PIN_PCINT1 PINC
#define PIN_PCINT2 PIND

#else
#error "please set up this part"
#endif

/*
   inline regtype &pinToPortRegOut(uint8_t pin) {
   switch (pin >> 3) {
   case 0: return PORTB;
   case 1: return PORTC;
   case 2: return PORTD;
   default: return fatal(FATAL_BAD_PIN_ARG), *(regtype *)0;
   }
   }
 */

/*
   inline regtype const &pinToPortRegIn(uint8_t pin) {
   switch (pin >> 3) {
   case 0: return PINB;
   case 1: return PINC;
   case 2: return PIND;
   default: return fatal(FATAL_BAD_PIN_ARG), *(regtype *)0;
   }
   }
 */

/*
   inline regtype &pinToPortDirReg(uint8_t pin) {
   switch (pin >> 3) {
   case 0: return DDRB;
   case 1: return DDRC;
   case 2: return DDRD;
   default: return fatal(FATAL_BAD_PIN_ARG), *(regtype *)0;
   }
   }
 */

/*
   inline regtype &pcMaskReg(uint8_t pin) {
   switch (pin >> 3) {
   case 0: return PCMSK0;
   case 1: return PCMSK1;
   case 2: return PCMSK2;
   default: return fatal(FATAL_BAD_PIN_ARG), *(regtype *)0;
   }
   }
 */

inline uint8_t pcMaskBit(uint8_t pin) {
    return (1 << (pin & 7));
}

/*
   inline uint8_t pcCtlBit(uint8_t pin) {
   switch (pin >> 3) {
   case 0: return (1 << PCIE0);
   case 1: return (1 << PCIE1);
   case 2: return (1 << PCIE2);
   default: return fatal(FATAL_BAD_PIN_ARG), 0;
   }
   }
 */

inline uint8_t digitalPinToPort(uint8_t pin) {
    //  do the translation elsewhere
    return pin;
}

inline regtype *portOutputRegister(uint8_t pin) {
    return &pinToPortRegOut(pin);
}

#define digitalPinToBitMask(pin) \
    (1 << ((pin) & 7))
/*
   inline regtype digitalPinToBitMask(uint8_t pin) {
   return (1 << (pin & 7));
   }
 */

#define pinMode(pin, output) \
    ((output) ? \
     (pinToPortDirReg(pin) |= digitalPinToBitMask(pin)) : \
     (pinToPortDirReg(pin) &= ~digitalPinToBitMask(pin)))
/*
   inline void pinMode(uint8_t pin, bool output) {
   if (output) {
   sbi(&pinToPortDirReg(pin), digitalPinToBitMask(pin));
   }
   else {
   cbi(&pinToPortDirReg(pin), digitalPinToBitMask(pin));
   }
   }
 */

#define digitalWrite(pin, output) \
    ((output) ? \
     (pinToPortRegOut(pin) |= digitalPinToBitMask(pin)) : \
     (pinToPortRegOut(pin) &= ~digitalPinToBitMask(pin)))
/*
   inline void digitalWrite(uint8_t pin, bool output) {
   if (output) {
   sbi(&pinToPortRegOut(pin), digitalPinToBitMask(pin));
   }
   else {
   cbi(&pinToPortRegOut(pin), digitalPinToBitMask(pin));
   }
   }
 */

#define digitalRead(pin) \
    ((pinToPortRegIn(pin) & digitalPinToBitMask(pin)) ? HIGH : LOW)
/*
   inline uint8_t digitalRead(uint8_t pin) {
   return (pinToPortRegIn(pin) & digitalPinToBitMask(pin)) ? HIGH : LOW;
   }
 */

inline uint8_t disable_interrupts() {
    uint8_t ret = SREG & 0x80;
    cli();
    return ret;
}

inline void restore_interrupts(uint8_t val) {
    if (val) {
        sei();
    }
}

class IntDisable
{
    public:
        IntDisable() { idi = disable_interrupts(); }
        ~IntDisable() { restore_interrupts(idi); }
        uint8_t idi;
};


#endif

