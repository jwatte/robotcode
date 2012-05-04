
#include <libavr.h>
#include <UTFT.h>
#include <UTFT.cpp>
#include <nRF24L01.h>





// PORTC == 8
//  ITDB02-2.2SP hooked up to C0=RST, C1=SDA, C2=SCL, C3=CS
//  UTFT class specifies RS(==SDA), WR(==SCL), CS, RST
UTFT<HX8340B_S> lcd(8|1, 8|2, 8|3, 8|0);


void on_twi_data(unsigned char size, void const *ptr)
{
}


nRF24L01<> rf;

ISR(PCINT2_vect)
{
  rf.onIRQ();
}


char hexchar(uint8_t v) {
  v = v & 0xf;
  if (v < 10) return '0' + v;
  return 'A' + (v - 10);
}

void phex(uint8_t val, uint8_t x, uint8_t y) {
  char buf[3] = { hexchar(val >> 4), hexchar(val), 0 };
  lcd.print(buf, x, y, 0);
}

uint8_t nWrite = 0;

void transmit_value(void *v)
{
  if (rf.canWriteData()) {
    if (!rf.hasLostPacket()) {
      v = (char *)v + 1;
      lcd.print("=", 1, 50, 0);
    }
    else {
      lcd.print("!", 1, 50, 0);
    }
    uint8_t val = (size_t)v & 0xff;
    ++nWrite;
    phex(nWrite, 100, 100);
    rf.writeData(1, &val);
  }
  after(100, &transmit_value, v);
}


void setup()
{
  twi_set_callback(on_twi_data);

  DDRB |= (1 << PB5);
  PORTB &= ~(1 << PB5);
  DDRC |= 0xf;

  lcd.InitLCD(LANDSCAPE);
  lcd.setBackColor(0, 0, 0);
  lcd.setColor(0, 0, 0);
  lcd.fillRect(0, 0, lcd.getDisplayXSize(), lcd.getDisplayYSize());
  lcd.setFont(SmallFont);
  lcd.setColor(192, 96, 0);
  lcd.print("E-Stop", 1, 1, 0);
  lcd.setColor(0, 0, 192);
  lcd.print("fatal 0x", 109, 1, 0);
  phex(eeprom_read_byte((uint8_t const *)EE_FATALCODE), 172, 1);
  lcd.print("reset 0x", 109, 16, 0);
  phex(eeprom_read_byte((uint8_t const *)EE_RESETSOURCE), 172, 16);
  lcd.print("prevr 0x", 109, 31, 0);
  phex(eeprom_read_byte((uint8_t const *)EE_PREV_RESETSOURCE), 172, 31);
  lcd.print("nboot 0x", 109, 46, 0);
  word w = eeprom_read_word((uint16_t const *)EE_NUM_BOOTS);
  phex(w >> 8, 172, 46);
  phex(w, 188, 46);

  rf.setup(50, 50);

  transmit_value(0);
}




