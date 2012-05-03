
#include <avr/io.h>
#include "spi.h"

#define DDRSPI DDRB
#define DD_SCK PB5
#define DD_MISO PB4
#define DD_MOSI PB3
#define DD_SS PB2

void enable_spi()
{
    PRR &= ~(1 << PRSPI);
    DDRSPI |= (1 << DD_MOSI) | (1 << DD_SCK) | (1 << DD_SS);
    DDRSPI &= ~(1 << DD_MISO);
    //  full rate -- fclk / 2
    SPSR = (1 << SPI2X);
    SPCR = (1 << SPE) | (1 << MSTR);
}

void disable_spi()
{
  SPCR &= ~(1 << SPE);
  PRR |= (1 << PRSPI);
}

uint8_t shift_spi(uint8_t val)
{
  SPDR = val;
  while (!(SPSR & (1 << SPIF))) {
    /* busy wait */;
  }
  return SPDR;
}

