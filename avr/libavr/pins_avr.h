#if !defined(pins_avr_h)
#define pins_avr_h

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

inline regtype &pinToPortRegOut(uint8_t pin) {
  switch (pin >> 3) {
  case 0: return PORTB;
  case 1: return PORTC;
  case 2: return PORTD;
  default: fatal(FATAL_BAD_PIN_ARG);
  }
}

inline regtype const &pinToPortRegIn(uint8_t pin) {
  switch (pin >> 3) {
  case 0: return PINB;
  case 1: return PINC;
  case 2: return PIND;
  default: fatal(FATAL_BAD_PIN_ARG);
  }
}

inline regtype &pinToPortDirReg(uint8_t pin) {
  switch (pin >> 3) {
  case 0: return DDRB;
  case 1: return DDRC;
  case 2: return DDRD;
  default: fatal(FATAL_BAD_PIN_ARG);
  }
}

inline regtype &pcMaskReg(uint8_t pin) {
  switch (pin >> 3) {
  case 0: return PCMSK0;
  case 1: return PCMSK1;
  case 2: return PCMSK2;
  default: fatal(FATAL_BAD_PIN_ARG);
  }
}

inline uint8_t pcMaskBit(uint8_t pin) {
  return (1 << (pin & 7));
}

inline regtype &pcCtlReg(uint8_t pin) {
  return PCICR;
}

inline uint8_t pcCtlBit(uint8_t pin) {
  switch (pin >> 3) {
  case 0: return (1 << PCIE0);
  case 1: return (1 << PCIE1);
  case 2: return (1 << PCIE2);
  default: fatal(FATAL_BAD_PIN_ARG);
  }
}

inline uint8_t digitalPinToPort(uint8_t pin) {
  //  do the translation elsewhere
  return pin;
}

inline regtype *portOutputRegister(uint8_t pin) {
  return &pinToPortRegOut(pin);
}

inline regtype digitalPinToBitMask(uint8_t pin) {
  return (1 << (pin & 7));
}

inline void pinMode(uint8_t pin, bool output) {
  if (output) {
    sbi(&pinToPortDirReg(pin), digitalPinToBitMask(pin));
  }
  else {
    cbi(&pinToPortDirReg(pin), digitalPinToBitMask(pin));
  }
}

inline void digitalWrite(uint8_t pin, bool output) {
  if (output) {
    sbi(&pinToPortRegOut(pin), digitalPinToBitMask(pin));
  }
  else {
    cbi(&pinToPortRegOut(pin), digitalPinToBitMask(pin));
  }
}

inline uint8_t digitalRead(uint8_t pin) {
  return (pinToPortRegIn(pin) & digitalPinToBitMask(pin)) ? HIGH : LOW;
}

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

