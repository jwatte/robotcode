
#include <libavr.h>
#include <UTFT.h>
#include <UTFT.cpp>
#include <nRF24L01.h>

#include "cmds.h"



#define PIN_BUTTON_STOP (16|7)
#define PIN_BUTTON_GO (0|0)
#define PIN_SELECTOR_LEFT (0|1)
#define PIN_SELECTOR_RIGHT (0|2)


// PORTC == 8
//  ITDB02-2.2SP hooked up to C0=RST, C1=SDA, C2=SCL, C3=CS
//  UTFT class specifies RS(==SDA), WR(==SCL), CS, RST
UTFT<HX8340B_S> lcd(8|1, 8|2, 8|3, 8|0);


void on_twi_data(unsigned char size, void const *ptr)
{
}


nRF24L01<> rf;

unsigned char uiMode = 0x80;
volatile unsigned char g_go_allowed = false;
char g_steer_adjust = 0;
char g_power_adjust = 0x80;
bool g_select_mode = false;

unsigned char volatile *adjust_ptr[] = {
  &uiMode,
  (unsigned char volatile *)&g_steer_adjust,
  (unsigned char volatile *)&g_power_adjust
};

unsigned char nIsConnected;

volatile bool g_go_pressed = false;
volatile unsigned char selectorValue = 0;
volatile unsigned char prevQuad = 0;
volatile unsigned char lastQuad = 0;
volatile char nInt = 0;

ISR(PCINT2_vect)
{
  rf.onIRQ();

  if (digitalRead(PIN_BUTTON_STOP) == LOW) {
    g_go_allowed = false;
    //  reset UI back to defaults
    g_select_mode = false;
    uiMode = 0;
  }
}

ISR(PCINT0_vect)
{
  ++nInt;

  unsigned char pb = PINB;
  if (!(pb & 1)) {
    if (uiMode == 0) {
      g_go_allowed = true;
    }
    else {
      g_go_pressed = true;
    }
  }

  unsigned char quad = ((pb & 4) ? 2 : 0) | ((pb & 2) ? 1 : 0);
  if (pb & 4) quad ^= 1;

  lastQuad = quad;
  if (((quad - 1) & 3) == prevQuad) {
    prevQuad = quad;
    ++selectorValue;
  }
  else if (((quad + 1) & 3) == prevQuad) {
    prevQuad = quad;
    --selectorValue;
  }
}

#define N_ROWS 12
#define N_COLS 27
char lcdChars[N_ROWS][N_COLS];

inline unsigned char color_r(unsigned char y) {
  return 255 - ((y > 128) ? (255 - y) : y);
}
inline unsigned char color_g(unsigned char y) {
  return 255 - ((y > 96) ? (y - 96) : (96 - y));
}
inline unsigned char color_b(unsigned char y) {
  return 255 - ((y > 192) ? (y - 192) : (192 - y));
}

void lcdDrawChars(char *ptr, unsigned char n, unsigned char y, unsigned char x)
{
  unsigned char cix = 0;
  if (y > 50) {
    cix = 1;
  }
  if (y > 100) {
    cix = 2;
  }
  lcd.setColor(color_r(y), color_g(y), color_b(y));
  char buf[N_COLS+1];
  memcpy(buf, ptr, n);
  buf[n] = 0;
  lcd.print(buf, x, y, 0);
}

void lcdPrint(unsigned char row, unsigned char col, char const *data)
{
  if (row >= N_ROWS || col >= N_COLS) {
    fatal(FATAL_BAD_PARAM);
  }
  bool diff = false;
  unsigned char startCol = col;
  char *ptr = &lcdChars[row][col];
  while (*data && (col < N_COLS)) {
    char ch = *data;
    ++data;
    if ((ch == *ptr) || ((ch == ' ') && (*ptr == 0))) {
      if (diff) {
        diff = false;
        lcdDrawChars(&lcdChars[row][startCol], col - startCol, row * 14, startCol * 8);
      }
    }
    else {
      *ptr = ch;
      if (!diff) {
        diff = true;
        startCol = col;
      }
    }
    ++col;
    ++ptr;
  }
  if (diff) {
    lcdDrawChars(&lcdChars[row][startCol], col - startCol, row * 14, startCol * 8);
  }
}

void update_param(cmd_parameter_value const &cpv)
{
  char buf[N_COLS];
  get_param_name((ParameterName)cpv.parameter, N_COLS, buf);
  buf[8] = 0;
  lcdPrint(cpv.parameter, 0, buf);
  format_value(cpv, N_COLS, buf);
  buf[N_COLS-1] = 0;
  lcdPrint(cpv.parameter, 8, buf);
  if (cpv.parameter == ParamTuneSteering) {
    g_steer_adjust = cpv.value[0];
  }
  else if (cpv.parameter == ParamTunePower) {
    g_power_adjust = cpv.value[0];
  }
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
    csg.go = g_go_allowed;
    rf.writeData(sizeof(csg), &csg);
  }
  after(100, &transmit_value, v);
  update_is_connected();
}

char const *uiModeNames[] = {
  "Monitor Bot ",
  "Adjust Steer",
  "Adjust Power"
};

void read_ui(void *)
{
  lcdPrint(9, 0, g_go_allowed ? "Can Go   " : "Must Stop");
  unsigned char oldUiMode = uiMode;
  {
    IntDisable idi;
    if (selectorValue < 0xfd && selectorValue > 3) {
      char d = -1;
      if (selectorValue < 0x80) {
        d = 1;
      }
      if (!g_select_mode) {
        uiMode += d;
      }
      else {
        *(adjust_ptr[uiMode]) += d;
      }
      selectorValue = 0;
    }
  }
  if (uiMode > 0xfc) {
    uiMode = sizeof(uiModeNames)/sizeof(uiModeNames[0]) - 1;
  }
  else if (uiMode >= sizeof(uiModeNames)/sizeof(uiModeNames[0])) {
    uiMode = 0;
  }
  if (uiMode != oldUiMode) {
    g_select_mode = false;
    lcdPrint(10, 0, uiModeNames[uiMode]);
    oldUiMode = uiMode;
  }
  if (g_go_pressed) {
    g_go_pressed = false;
    g_select_mode = !g_select_mode;
  }
  if (uiMode == 1) {
    char hex[16] = {
      hexchar(g_steer_adjust >> 4),
      hexchar(g_steer_adjust & 0xf),
      0
    };
    lcdPrint(11, 8, hex);
    get_param_name(ParamTuneSteering, 16, hex);
    lcdPrint(11, 0, hex);
    if (rf.canWriteData()) {
      cmd_parameter_value *cpv = (cmd_parameter_value *)hex;
      cpv->cmd = CMD_PARAMETER_VALUE;
      cpv->fromNode = NodeEstop;
      cpv->toNode = NodeMotorPower;
      cpv->parameter = ParamTuneSteering;
      cpv->type = TypeByte;
      cpv->value[0] = g_steer_adjust;
      rf.writeData(param_size(*cpv), cpv);
    }
  }
  else if (uiMode == 2) {
    char hex[16] = {
      hexchar(g_power_adjust >> 4),
      hexchar(g_power_adjust & 0xf),
      0
    };
    lcdPrint(11, 8, hex);
    get_param_name(ParamTunePower, 16, hex);
    lcdPrint(11, 0, hex);
    if (rf.canWriteData()) {
      cmd_parameter_value *cpv = (cmd_parameter_value *)hex;
      cpv->cmd = CMD_PARAMETER_VALUE;
      cpv->fromNode = NodeEstop;
      cpv->toNode = NodeMotorPower;
      cpv->parameter = ParamTunePower;
      cpv->type = TypeByte;
      cpv->value[0] = g_power_adjust;
      rf.writeData(param_size(*cpv), cpv);
    }
  }
  else {
    char empty[16];
    memset(empty, 32, 15);
    empty[15] = 0;
    lcdPrint(11, 0, empty);
    g_select_mode = false;
  }
  if (g_select_mode) {
    lcdPrint(11, 15, "<->");
  }
  else {
    lcdPrint(11, 15, "   ");
  }
  g_go_pressed = false;
  after(30, &read_ui, 0);
}

void setup()
{
  twi_set_callback(on_twi_data);

  DDRB |= (1 << PB5);
  PORTB &= ~(1 << PB5);
  DDRC |= 0xf;
  DDRD |= (1 << PD3);
  PORTD |= (1 << PD3);
  DDRB &= ~0x7;   //  go, selector
  PORTB |= 0x7;   //  pull-up
  DDRD &= ~0x80;  //  stop
  PORTD |= 0x80;  //  pull-up

  PCMSK0 |= 0x7;  //  go, selector
  PCICR |= 0x1;   //  port B pin change

  lcd.InitLCD(LANDSCAPE);
  lcd.setBackColor(0, 0, 0);
  lcd.setColor(0, 0, 0);
  lcd.fillRect(0, 0, lcd.getDisplayXSize(), lcd.getDisplayYSize());
  lcd.setFont(SmallFont);
  lcd.setColor(0, 255, 0);

  rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);

  transmit_value(0);
  read_ui(0);
}




