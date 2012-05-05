
#include <libavr.h>
#include <UTFT.h>
#include <UTFT.cpp>
#include <nRF24L01.h>

#include "cmds.h"




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
int oldMotorPower = 0;
bool isOk = false;

char mcbs[10] = { 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, 0xfd, };

void display_motor_crash_bits(int n, char const *buf)
{
  if (n > sizeof(mcbs)) {
    n = sizeof(mcbs);
  }
  lcd.setColor(192, 0, 192);
  for (int i = 0; i < n; ++i) {
    if (mcbs[i] != buf[i]) {
      mcbs[i] = buf[i];
      phex(buf[i], i * 18, 135);
    }
  }
  lcd.setColor(0, 0, 192);
}


void transmit_value(void *v)
{
  uint8_t hd = rf.hasData();
  if (hd) {
    char buf[32];
    rf.readData(hd, buf);
    int curMotorPower = oldMotorPower;
    switch (buf[0]) {
      case CMD_MOTOR_POWER:
        curMotorPower = (((int)buf[1] << 8) & 0xff00) | (buf[2] & 0xff);
        break;
      case CMD_MOTOR_CRASH_BITS:
        display_motor_crash_bits(hd, &buf[1]);
        break;
      default:
        lcd.setColor(192, 96, 0);
        phex(buf[0], 40, 150);
        break;
    }
    if (curMotorPower != oldMotorPower) {
      phex(curMotorPower, 70, 150);
      oldMotorPower = curMotorPower;
    }
  }
  if (rf.canWriteData()) {
    if (!rf.hasLostPacket()) {
      v = (char *)v + 1;
      if (!isOk) {
        lcd.setColor(0, 192, 0);
        lcd.print("ok!", 1, 50, 0);
        isOk = true;
      }
    }
    else if (isOk) {
      lcd.setColor(192, 0, 0);
      lcd.print("n/c", 1, 50, 0);
      isOk = false;
    }
    uint8_t val = (size_t)v & 0xff;
    ++nWrite;
    lcd.setColor(128, 128, 128);
    phex(nWrite, 100, 100);
    rf.writeData(1, &val);
  }
  lcd.setColor(0, 0, 192);
  after(100, &transmit_value, v);
}


unsigned short oldTimer;

void write_timer(void *)
{
  unsigned short ms = read_timer();
  lcd.setColor(128, 128, 128);
  phex(ms >> 8, 10, 85);
  phex(ms & 0xff, 26, 85);
  after(0, &write_timer, 0);
  if (ms - oldTimer > 2000 && oldTimer != 0) {
    lcd.setColor(255, 255, 255);
    phex(ms >> 8, 50, 85);
    phex(ms & 0xff, 66, 85);
    phex(oldTimer >> 8, 90, 85);
    phex(oldTimer & 0xff, 106, 85);
  }
  lcd.setColor(0, 0, 192);
  oldTimer = ms;
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

  rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);

  transmit_value(0);
  write_timer(0);
}




