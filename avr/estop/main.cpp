
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

unsigned char nIsConnected;

ISR(PCINT2_vect)
{
  rf.onIRQ();
}


void update_param(cmd_parameter_value const &cpv)
{
  char buf[32];
  get_param_name((ParameterName)cpv.parameter, 32, buf);
  lcd.setColor(128, 128, 128);
  lcd.print(buf, 0, 15 * cpv.parameter, 0);
  format_value(cpv, 32, buf);
  lcd.setColor(192, 192, 192);
  lcd.print(buf, 60, 15 * cpv.parameter, 0);
}

bool wasConnected = true;

void update_is_connected()
{
  bool isConnected = nIsConnected > 0;
  if (isConnected != wasConnected) {
    if (isConnected) {
      lcd.setColor(0, 255, 128);
    }
    else {
      lcd.setColor(255, 128, 0);
    }
    lcd.print(isConnected ? "OK!" : "n/c", 195, 0, 0);
    wasConnected = isConnected;
  }
}

void dispatch_cmd(cmd_hdr const &hdr)
{
  if (hdr.toNode != NodeAny && hdr.toNode != NodeEstop) {
    return;
  }
  switch (hdr.cmd) {
    case CMD_PARAMETER_VALUE:
      update_param((cmd_parameter_value const &)hdr);
      nIsConnected = 10;
      break;
  }
}

void transmit_value(void *v)
{
  uint8_t hd = rf.hasData();
  if (nIsConnected > 0) {
    --nIsConnected;
  }
  if (hd) {
    union {
      char buf[32];
      cmd_hdr hdr;
    } u;
    rf.readData(hd, u.buf);
    dispatch_cmd(u.hdr);
  }
  if (rf.canWriteData()) {
    if (!rf.hasLostPacket()) {
      v = (char *)v + 1;
    }
    cmd_stop_go csg;
    csg.cmd = CMD_STOP_GO;
    csg.fromNode = NodeEstop;
    csg.toNode = NodeMotorPower;
    csg.go = true;
    rf.writeData(sizeof(csg), &csg);
  }
  after(100, &transmit_value, v);
  update_is_connected();
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
  lcd.setColor(192, 64, 255);

  rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);

  transmit_value(0);
}




