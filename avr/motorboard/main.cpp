
#include <libavr.h>
#include <nRF24L01.h>

#include "cmds.h"


#define LED_GO_B (1 << PB0)
#define LED_PAUSE_D (1 << PD7)

nRF24L01<true, 0|7, 16|4, 0|6> rf;

ISR(PCINT0_vect)
{
  rf.onIRQ();
}
uint8_t nRadioContact;

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
  if (!nRadioContact) {
    power = 0;
  }
  if ((power > 0 && g_motor_actual_power < 0) || (power < 0 && g_motor_actual_power > 0)) {
    PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_A_NCH_D));
    PORTB = (PORTB & ~(MOTOR_B_PCH_B | MOTOR_B_NCH_B));
    udelay(10); // prevent shooth-through
  }
  g_motor_actual_power = power;
  if (power == 0) {
    //  ground everything out
    PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
    PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
    if (nRadioContact) {
      set_led_state(true, false, 0);
    }
    else {
      set_led_state(true, false, 800);
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
  if (rf.canWriteData()) {
    char mpCmd[3] = { CMD_MOTOR_POWER, power >> 8, power };
    rf.writeData(3, mpCmd);
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
  after(500, &set_motor, (void *)((int)v + 1));
}

void set_servo(void *v)
{
  //  todo: implement me
}



void on_twi_data(unsigned char size, void const *ptr)
{
}

void poll_radio(void *)
{
  if (rf.hasData()) {
    char buf[32];
    rf.readData(32, buf);
    nRadioContact = 10;
  }
  else if (nRadioContact > 0) {
    --nRadioContact;
  }
  update_motor_power();
  after(50, &poll_radio, 0);
}

void slow_bits_update(void *v)
{
  if (rf.canWriteData()) {
    char cmd[11] = {
      CMD_MOTOR_CRASH_BITS
    };
    eeprom_read_block(&cmd[1], (void const *)0, sizeof(cmd)-1);
    rf.writeData(sizeof(cmd), cmd);
  }
  after(4785, &slow_bits_update, 0);
}

void setup()
{
  /* status lights are outputs */
  setup_leds();
  setup_motors();
  delay(100); //  wait for radio to boot
  rf.setup(ESTOP_RF_CHANNEL, ESTOP_RF_ADDRESS);
  twi_set_callback(on_twi_data);
  //  kick off the chain of tasks
  set_servo(0);
  set_motor(0);
  poll_radio(0);
  slow_bits_update(0);
}


