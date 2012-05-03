
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

uint8_t totalBits = 0;

void transmit_value(void *v)
{
  if (rf.canWriteData()) {
    if (!rf.hasLostPacket()) {
      v = (char *)v + 1;
      phex((int)v, 10, 100);
    }
    uint8_t val = (size_t)v & 0xff;
    rf.writeData(1, &val);
  }
  else {
    lcd.print("! ", 10, 100, 0);
  }
  uint8_t bits = rf.readClearDebugBits();
  totalBits |= bits;
  phex(bits, 10, 120);
  phex(totalBits, 50, 120);
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
  lcd.setFont(BigFont);
  lcd.setColor(192, 96, 0);
  lcd.print("E-Stop", 1, 1, 0);
  lcd.setFont(SmallFont);
  lcd.setColor(0, 0, 192);
  lcd.print("fatal 0x", 100, 25, 0);
  phex(eeprom_read_byte((uint8_t const *)EE_FATALCODE), 162, 25);
  lcd.print("reset 0x", 100, 40, 0);
  phex(eeprom_read_byte((uint8_t const *)EE_RESETSOURCE), 162, 40);
  lcd.print("prevr 0x", 100, 55, 0);
  phex(eeprom_read_byte((uint8_t const *)EE_PREV_RESETSOURCE), 162, 55);

  lcd.print("radio", 10, 70, 0);
  rf.setup(50, 50);
  lcd.print("start", 10, 85, 0);

  transmit_value(0);
}




