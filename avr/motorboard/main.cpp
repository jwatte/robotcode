
#include <libavr.h>
#include <nRF24L01.h>

#include "cmds.h"


/* For some reason, running the servo on PWM is not very clean. */
/* Perhaps an approach that uses timer1 interrupts for high resolution */
/* would be better. But for now, I just schedule 50 Hz updates. */
#define USE_SERVO_TIMER 0

#define LED_GO_B (1 << PB0)
#define LED_PAUSE_D (1 << PD7)

nRF24L01<true, 0|7, 16|4, 0|6> rf;

ISR(PCINT0_vect)
{
  rf.onIRQ();
}
uint8_t nRadioContact = 1;


struct TuneStruct {
  char d_steer;
  unsigned char cksum;
};
TuneStruct g_tuning;

unsigned char calc_cksum(unsigned char sz, void const *src)
{
  unsigned char cksum = 0x55;
  unsigned char const *d = (unsigned char const *)src;
  for (unsigned char ix = 0; ix != sz; ++ix) {
    cksum = ((cksum << 1) + ix) ^ d[ix];
  }
  return cksum;
}

void write_tuning()
{
  g_tuning.cksum = calc_cksum(sizeof(g_tuning)-1, &g_tuning);
  eeprom_write_block(&g_tuning, (void *)EE_TUNING, sizeof(g_tuning));
}

void read_tuning()
{
  eeprom_read_block(&g_tuning, (void const *)EE_TUNING, sizeof(g_tuning));
  if (g_tuning.cksum != calc_cksum(sizeof(g_tuning)-1, &g_tuning)) {
    memset(&g_tuning, 0, sizeof(g_tuning));
    g_tuning.d_steer = -10;
    write_tuning();
  }
}




int g_led_timer;
bool g_led_paused;
bool g_led_go;
bool g_led_blink;
bool g_after;

void blink_leds(bool on)
{
  DDRB |= LED_GO_B;
  DDRD |= LED_PAUSE_D;
  if (on) {
    PORTB |= LED_GO_B;
    PORTD |= LED_PAUSE_D;
  }
  else {
    PORTB &= ~LED_GO_B;
    PORTD &= ~LED_PAUSE_D;
  }
}

void setup_leds()
{
  blink_leds(false);
  fatal_set_blink(&blink_leds);
}

void update_leds(void *)
{
  g_after = false;
  if (g_led_timer) {
    g_led_blink = !g_led_blink;
  }
  else {
    g_led_blink = true;
  }
  if (g_led_go && g_led_blink) {
    PORTB = PORTB | LED_GO_B;
  }
  else {
    PORTB = PORTB & ~LED_GO_B;
  }
  if (g_led_paused && g_led_blink) {
    PORTD = PORTD | LED_PAUSE_D;
  }
  else {
    PORTD = PORTD & ~LED_PAUSE_D;
  }
  if (g_led_timer && !g_after) {
    g_after = true;
    after(g_led_timer, &update_leds, 0);
  }
}

void set_led_state(bool paused, bool go, int blink)
{
  g_led_timer = blink;
  g_led_paused = paused;
  g_led_go = go;
  if (!g_after) {
    g_after = true;
    after(0, &update_leds, 0);
  }
}



#define MOTOR_A_PCH_D (1 << PD5)
#define MOTOR_A_NCH_D (1 << PD6)
#define MOTOR_B_PCH_B (1 << PB2)
#define MOTOR_B_NCH_B (1 << PB1)

int g_motor_actual_power;
int g_motor_desired_power;
unsigned char g_motor_allowed;
unsigned char g_local_stop = true;

void setup_motors()
{
  PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_A_NCH_D));
  PORTB = (PORTB & ~(MOTOR_B_PCH_B | MOTOR_B_NCH_B));
  DDRD |= (MOTOR_A_PCH_D | MOTOR_A_NCH_D);
  DDRB |= (MOTOR_B_PCH_B | MOTOR_B_NCH_B);
}

void update_motor_power()
{
  int power = g_motor_desired_power;
  if (!nRadioContact || !g_motor_allowed || g_local_stop) {
    power = 0;
  }
  if ((power > 0 && g_motor_actual_power < 0) ||
      (power < 0 && g_motor_actual_power > 0)) {
    PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_A_NCH_D));
    PORTB = (PORTB & ~(MOTOR_B_PCH_B | MOTOR_B_NCH_B));
    udelay(10); // prevent shooth-through
  }
  g_motor_actual_power = power;
  if (power == 0) {
    //  ground everything out
    PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
    PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
    if (!nRadioContact) {
      set_led_state(true, false, 1200);
    }
    else if (!g_motor_allowed || g_local_stop) {
      set_led_state(true, false, 400);
    }
    else {
      set_led_state(true, false, 0);
    }
  }
  else if (power < 0) {
    //  negative A, positive B
    PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
    PORTB = (PORTB & ~(MOTOR_B_NCH_B)) | MOTOR_B_PCH_B;
    set_led_state(false, true, 200);
  }
  else {
    //  positive A, negative B
    PORTD = (PORTD & ~(MOTOR_A_NCH_D)) | MOTOR_A_PCH_D;
    PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
    set_led_state(false, true, 0);
  }
}

void set_motor_power(int power)
{
  g_motor_desired_power = power;
  update_motor_power();
}

void set_motor(void *v)
{
  switch ((int)v) {
  default:
    v = 0;
  case 0:
    set_motor_power(0);
    break;
  case 1:
    set_motor_power(192);
    break;
  case 2:
    set_motor_power(-255);
    break;
  case 3:
    set_motor_power(255);
    break;
  }
  after(3000, &set_motor, (void *)((int)v + 1));
}

unsigned char g_steering_angle = 90;

unsigned char tuned_angle()
{
  int i = (int)g_steering_angle + g_tuning.d_steer;
  if (i < 0) i = 0;
  if (i > 180) i = 180;
  return (unsigned char)i;
}

#if USE_SERVO_TIMER
void set_servo_timer_angle(unsigned char a)
{
  //  this is very approximately the angle
  OCR2B = tuned_angle() / 3 + 16;
}
#endif

void setup_servo()
{
  DDRD |= (1 << PD3);
  PORTD &= ~(1 << PD3);
#if USE_SERVO_TIMER
  power_timer2_enable();
  ASSR = 0;
  //  Fast PWM on COM2B1 pin
  TCCR2A = (1 << COM2B1) | (0 << COM2B0) | (1 << WGM21) | (1 << WGM20);
  //  120 Hz
  TCCR2B = (1 << CS22) | (1 << CS21) | (0 << CS20);
  set_servo_timer_angle(g_steering_angle);
#endif
}

void update_servo(void *v)
{
#if USE_SERVO_TIMER
  //  this is very approximate
  set_servo_timer_angle(g_steering_angle);
#else
  {
    IntDisable idi;
    PORTD |= (1 << PD3);
    udelay(tuned_angle() * 11U + 580U);
    PORTD &= ~(1 << PD3);
  }
#endif
  //  2 ms spin delay every 40 ms is 5% of available CPU...
  after(40, &update_servo, 0);
}


void setup_buttons()
{
  DDRD &= ~(1 << PD2);
  PORTD |= (1 << PD2);
}

void poll_button(void *)
{
  if (!(PIND & (1 << PD2))) {
    g_local_stop++;
  }
  else if (g_local_stop > 20) {
    //  start again by holding for 2 seconds
    g_local_stop = 0;
  }
  else if (g_local_stop > 0) {
    g_local_stop = 1;
  }
  after(100, &poll_button, 0);
}


void on_twi_data(unsigned char size, void const *ptr)
{
}

void tune_steering(cmd_parameter_value const &cpv)
{
  g_tuning.d_steer = cpv.value[0];
  write_tuning();
}

void dispatch_cmd(unsigned char n, char const *data)
{
  cmd_hdr const &hdr = *(cmd_hdr const *)data;
  if (hdr.toNode == NodeMotorPower) {
    if (hdr.cmd == CMD_STOP_GO) {
      g_motor_allowed = ((cmd_stop_go const &)hdr).go;
    }
    else if (hdr.cmd == CMD_PARAMETER_VALUE) {
      cmd_parameter_value const &cpv = (cmd_parameter_value const &)hdr;
      switch (cpv.parameter) {
      case ParamTuneSteering:
        tune_steering(cpv);
        break;
      }
    }
  }
}

void reset_radio(void *)
{
  if (!nRadioContact) {
    set_led_state(false, false, 0);
    blink_leds(true);
    rf.teardown();
    blink_leds(false);
    delay(100);
    blink_leds(true);
    rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
    blink_leds(false);
    after(8000, &reset_radio, 0);
  }
}

void poll_radio(void *)
{
  unsigned char n = rf.hasData();
  if (n > 0) {
    nRadioContact = 20;
    char buf[32];
    rf.readData(n, buf);
    dispatch_cmd(n, buf);
  }
  else if (nRadioContact > 0) {
    --nRadioContact;
    if (nRadioContact == 0) {
      after(100, &reset_radio, 0);
    }
  }
  update_motor_power();
  after(50, &poll_radio, 0);
}


unsigned char curSendParam = 0;

unsigned char param_size(cmd_parameter_value const &cpv)
{
  switch (cpv.type) {
    case TypeNone:
      return sizeof(cpv) - sizeof(cpv.value);
    case TypeByte:
      return sizeof(cpv) - sizeof(cpv.value) + 1;
    case TypeShort:
      return sizeof(cpv) - sizeof(cpv.value) + 2;
    case TypeLong:
      return sizeof(cpv) - sizeof(cpv.value) + 4;
    default:  //  send everything
      return sizeof(cpv);
  }
}

void set_value(cmd_parameter_value &cpv, unsigned char ch)
{
  cpv.type = TypeByte;
  cpv.value[0] = ch;
}

void set_value(cmd_parameter_value &cpv, char ch)
{
  cpv.type = TypeByte;
  cpv.value[0] = ch;
}

void set_value(cmd_parameter_value &cpv, int ch)
{
  cpv.type = TypeShort;
  cpv.value[0] = (unsigned char)(ch & 0xff);
  cpv.value[1] = (unsigned char)((unsigned int)ch >> 8);
}

void set_value(cmd_parameter_value &cpv, unsigned int ch)
{
  cpv.type = TypeShort;
  cpv.value[0] = (unsigned char)(ch & 0xff);
  cpv.value[1] = (unsigned char)((unsigned int)ch >> 8);
}

void set_value(cmd_parameter_value &cpv, unsigned char len, void const *data)
{
  cpv.type = TypeRaw;
  if (len > sizeof(cpv.value)) {
    len = sizeof(cpv.value);
  }
  memcpy(cpv.value, data, len);
}


void send_param(unsigned char p)
{
  cmd_parameter_value cpv;
  cpv.cmd = CMD_PARAMETER_VALUE;
  cpv.fromNode = NodeMotorPower;
  cpv.toNode = NodeAny;
  cpv.parameter = p;

  switch (p) {
  case ParamGoAllowed:
    set_value(cpv, (unsigned char)(g_motor_allowed && !g_local_stop));
    break;
  case ParamMotorPower:
    set_value(cpv, (int)g_motor_desired_power);
    break;
  case ParamSteerAngle:
    set_value(cpv, (unsigned char)g_steering_angle);
    break;
  case ParamEEDump:
    eeprom_read_block(&cpv.value[1], (void const *)0, 12);
    cpv.value[0] = 12;
    cpv.type = TypeRaw;
    break;
  case ParamTuneSteering:
    set_value(cpv, (unsigned char)g_tuning.d_steer);
    break;
  }

  rf.writeData(param_size(cpv), &cpv);
}

void slow_bits_update(void *v)
{
  if (rf.canWriteData()) {
    send_param(curSendParam);
    curSendParam++;
    if (curSendParam == ParamMax) {
      curSendParam = 0;
    }
  }
  after(175, &slow_bits_update, 0);
}

void setup()
{
  read_tuning();
  setup_leds();
  setup_motors();
  setup_servo();
  setup_buttons();
  delay(100); //  wait for radio to boot
  rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
  twi_set_callback(on_twi_data);
  //  kick off the chain of tasks
  update_servo(0);
  set_motor(0);
  poll_radio(0);
  slow_bits_update(0);
  poll_button(0);
}


