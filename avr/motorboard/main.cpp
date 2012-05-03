
#define F_CPU 8000000UL

#include <inttypes.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include <util/twi.h>

#define disable_interrupts() cli()
#define enable_interrupts() sei()

#define EE_FATALCODE 2
#define EE_RESETSOURCE 4
#define EE_PREV_RESETSOURCE 5

/* fatal API */

enum
{
  FATAL_OUT_OF_AFTERS = 1,
  FATAL_BAD_DELAY_TIME = 2,
  FATAL_TASK_TOOK_TOO_LONG = 3,
  FATAL_TWI_NO_INFO = 4,
  FATAL_TWI_SEND_TOO_BIG = 5,
  FATAL_TOO_LONG_UDELAY = 6,
  FATAL_TWI_ERROR_BASE = 0x60
};

/* global volatile int to avoid optimization of spinloop */
int volatile globalctr;

/* tell the world about an emergency by flashing indicators */
void fatal(int err)
{
  disable_interrupts();
  //  remember what the last fatal code was
  if (eeprom_read_byte((uint8_t const *)EE_FATALCODE) != err) {
    eeprom_write_byte((uint8_t *)EE_FATALCODE, (unsigned char)err);
  }
  DDRB |= (1<<PB0); /* set PB0 to output */
  DDRD |= (1<<PD7); /* set PD7 to output */
  while (true)
  {
    if (err >= FATAL_TWI_ERROR_BASE) {
      /* just blink madly! */
      PORTB |= (1<<PB0);  /* blink on */
      PORTD |= (1<<PD7);
      for (int volatile i = 0; i < 100; ++i) {
        for (globalctr = 0; globalctr < 300; ++globalctr) {
        }
      }
      PORTB &= ~(1<<PB0); /* blink off */
      PORTD &= ~(1<<PD7);
      for (int volatile i = 0; i < 100; ++i) {
        for (globalctr = 0; globalctr < 300; ++globalctr) {
        }
      }
    }
    else {
      for (int k = 0; k < err; ++k) {
        PORTB |= (1<<PB0);  /* blink on */
        PORTD |= (1<<PD7);
        for (int volatile i = 0; i < 100; ++i) {
          for (globalctr = 0; globalctr < 500; ++globalctr) {
          }
        }
        PORTB &= ~(1<<PB0); /* blink off */
        PORTD &= ~(1<<PD7);
        for (int volatile i = 0; i < 100; ++i) {
          for (globalctr = 0; globalctr < 700; ++globalctr) {
          }
        }
      }
      for (int volatile i = 0; i < 500; ++i) {
        for (globalctr = 0; globalctr < 800; ++globalctr) {
        }
      }
    }
  }
}



/* timer API */

unsigned short g_timer_1ms;
unsigned char g_timer_8us;

//  I accumulate one ms and 24 us per interrupt, 
//  because 250 ticks == 1 ms, and I run 256 ticks.
//  Every 41.6 times, I've accumulated 1 full ms.
//  This means that I can count 8 microsecond ticks 
//  and keep a fully accurate clock.
ISR(TIMER0_OVF_vect)
{
  /* tuned for 8 MHz */
  if (g_timer_8us >= 124) {
    g_timer_8us -= 124;
    g_timer_1ms += 3;
  }
  else {
    g_timer_1ms += 2;
    g_timer_8us += 6;
  }
}

void setup_timers()
{
  //  timer 0 at 1000 Hz
  power_timer0_enable();
  TCCR0A = 0;
  TCCR0B = 3; //  system clock divided by 64 -- 125 kHz counter, so about 0.5 kHz clock
  TIMSK0 = 1; //  interrupt on overflow
}

short uread_timer()
{
  disable_interrupts();
  unsigned short l = g_timer_1ms;
  unsigned short sh = TCNT0 * 8 + g_timer_8us * 8;
  enable_interrupts();
  return l * 1000 + sh;
}

short read_timer()
{
  disable_interrupts();
  unsigned short l = g_timer_1ms;
  /* tcnt0 is 8 nanoseconds at 8 MHz clock */
  unsigned short sh = TCNT0 * 8 + g_timer_8us * 8;
  enable_interrupts();
  if (sh >= 1000) {
    if (sh >= 2000) {
      l += 2;
    }
    else {
      l += 1;
    }
  }
  return l;
}

void udelay(unsigned short amt)
{
  if (amt > 8000) {
    fatal(FATAL_TOO_LONG_UDELAY);
  }
  unsigned short now = uread_timer();
  while (uread_timer() - now < amt) {
    // do nothing
  }
}

/* task API */

//  how many milliseconds are allowed per task?
#define MAX_TASK_TIME 50

struct AfterRec
{
  unsigned short at_time;
  void (*func)(void *data);
  void *data;
};

#define MAX_AFTERS 16
AfterRec g_afters[MAX_AFTERS];

void at(unsigned short time, void (*func)(void *data), void *data)
{
  for (unsigned char ch = 0; ch != MAX_AFTERS; ++ch)
  {
    if (!g_afters[ch].func)
    {
      g_afters[ch].at_time = time;
      g_afters[ch].func = func;
      g_afters[ch].data = data;
      return;
    }
  }
  fatal(FATAL_OUT_OF_AFTERS);
}

void after(unsigned short delay, void (*func)(void *data), void *data)
{
  if (delay > 32767)
  {
    fatal(FATAL_BAD_DELAY_TIME);
  }
  at(read_timer() + delay, func, data);
}

void schedule()
{
  wdt_reset();
  unsigned short now = read_timer();
  for (unsigned char ch = 0; ch != MAX_AFTERS; ++ch)
  {
    if (!g_afters[ch].func)
    {
      continue;
    }
    if ((short)(now - g_afters[ch].at_time) >= 0)
    {
      void (*func)(void*) = g_afters[ch].func;
      void *data = g_afters[ch].data;
      g_afters[ch].func = 0;
      g_afters[ch].data = 0;
      (*func)(data);
      wdt_reset();
      unsigned short then = read_timer();
      if (then - now > MAX_TASK_TIME)
      {
        fatal(FATAL_TASK_TOOK_TOO_LONG);
      }
      now = then;
    }
  }
}



/* watchdog API */

void setup_watchdog()
{
  //  clear interrupt source flag
  unsigned char rsrc = MCUSR;
  MCUSR = 0;
  wdt_disable();
  unsigned char curCode = eeprom_read_byte((uint8_t const *)EE_RESETSOURCE);
  unsigned char prevCode = eeprom_read_byte((uint8_t const *)EE_PREV_RESETSOURCE);
  //  save current reset in curCode
  if (curCode != rsrc) {
    eeprom_write_byte((uint8_t *)EE_RESETSOURCE, rsrc);
  }
  //  save abnormal resets in prevCode
  if ((curCode & ((1 << WDRF) | (1 << BORF))) && (curCode != prevCode)) {
    eeprom_write_byte((uint8_t *)EE_PREV_RESETSOURCE, curCode);
  }
  // enable watchdog for system reset if timing out
  wdt_enable(WDTO_8S);
  wdt_reset();
}


/* TWI slave API */

#define TWI_ADDRESS 0x01
#define TWI_MAX_SIZE 0x10


unsigned char _twi_recv_buf[TWI_MAX_SIZE];
unsigned char _twi_recv_bufptr;
unsigned char _twi_buf[TWI_MAX_SIZE];
unsigned char _twi_ready;
unsigned char _twi_send_buf[TWI_MAX_SIZE];
unsigned char _twi_send_end;
unsigned char _twi_send_cur;
void (*_twi_callback)(unsigned char, void const *);

void setup_twi_slave()
{
  power_twi_enable();
  TWAR = TWI_ADDRESS;
  TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
}

void twi_set_callback(void (*func)(unsigned char, void const *))
{
  _twi_callback = func;
}

void twi_call_callback(void *)
{
  //  finally, acknowledge!
  //  I'm outside of interrupt handler code here.
  TWCR = (1 << TWINT) | (1 << TWEA);
  if (_twi_callback) {
    _twi_callback(_twi_ready, _twi_buf);
  }
}

ISR(TWI_vect)
{
  //  what state am I in?
  unsigned char status = TWSR & ~0x07;
  switch (status) {
  case TW_SR_SLA_ACK:
  case TW_SR_GCALL_ACK: {
      unsigned char code = (1 << TWINT);
      if (_twi_recv_bufptr < sizeof(_twi_recv_buf)) {
        code |= (1 << TWEA);
      }
      TWCR = code;  //  go ahead and send the byte
    }
    break;
  case TW_SR_DATA_ACK:
  case TW_SR_DATA_NACK: {
      unsigned char code = (1 << TWINT);
      if (_twi_recv_bufptr < sizeof(_twi_recv_buf)) {
        _twi_recv_buf[_twi_recv_bufptr] = TWDR;
        ++_twi_recv_bufptr;
        code |= (1 << TWEA);
      }
      TWCR = code;
    }
    break;
  case TW_SR_STOP: {
      memcpy(_twi_buf, _twi_recv_buf, _twi_recv_bufptr);
      _twi_ready = _twi_recv_bufptr;
      _twi_recv_bufptr = 0;
      after(0, twi_call_callback, 0);
    }
    break;
  case TW_NO_INFO:
    fatal(FATAL_TWI_NO_INFO);
  default:
  case TW_SR_GCALL_DATA_ACK:
  case TW_SR_GCALL_DATA_NACK:
  case TW_SR_ARB_LOST_SLA_ACK:
  case TW_SR_ARB_LOST_GCALL_ACK:
  case TW_BUS_ERROR:
    /* these should not happen */
    fatal(status);
    break;
  }
}

unsigned char twi_poll()
{
  return _twi_ready;
}

void *twi_getbuf()
{
  return _twi_buf;
}

bool twi_ready_to_send()
{
  return _twi_send_end == _twi_send_cur;
}

void twi_send(unsigned char bytes, void const *data)
{
  if (bytes > TWI_MAX_SIZE) {
    fatal(FATAL_TWI_SEND_TOO_BIG);
  }
  memcpy(_twi_send_buf, data, bytes);
  _twi_send_cur = 0;
  _twi_send_end = bytes;
  // todo: kick off a send here
}




/* boot stuff */

void setup_boot_code() {
  disable_interrupts();
  setup_watchdog();
  setup_timers();
  setup_twi_slave();
  enable_interrupts();
}



#define LED_GO_B (1 << PB0)
#define LED_PAUSE_D (1 << PD7)

int g_led_timer;
bool g_led_paused;
bool g_led_go;
bool g_led_blink;
bool g_after;

void setup_leds()
{
  DDRB |= LED_GO_B;
  DDRD |= LED_PAUSE_D;
}

void update_leds(void *)
{
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
  if (g_led_timer) {
    g_after = true;
    after(g_led_timer, &update_leds, 0);
  }
  else {
    g_after = false;
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

int g_motor_power;

void setup_motors()
{
  PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_A_NCH_D));
  PORTB = (PORTB & ~(MOTOR_B_PCH_B | MOTOR_B_NCH_B));
  DDRD |= (MOTOR_A_PCH_D | MOTOR_A_NCH_D);
  DDRB |= (MOTOR_B_PCH_B | MOTOR_B_NCH_B);
}

void set_motor_power(int power)
{
  if ((power > 0 && g_motor_power < 0) || (power < 0 && g_motor_power > 0)) {
    PORTD = (PORTD & ~(MOTOR_A_PCH_D | MOTOR_A_NCH_D));
    PORTB = (PORTB & ~(MOTOR_B_PCH_B | MOTOR_B_NCH_B));
    udelay(10); // prevent shooth-through
  }
  g_motor_power = power;
  if (power == 0) {
    //  ground everything out
    PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
    PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
  }
  else if (power < 0) {
    //  negative A, positive B
    PORTD = (PORTD & ~(MOTOR_A_PCH_D)) | MOTOR_A_NCH_D;
    PORTB = (PORTB & ~(MOTOR_B_NCH_B)) | MOTOR_B_PCH_B;
  }
  else {
    //  positive A, negative B
    PORTD = (PORTD & ~(MOTOR_A_NCH_D)) | MOTOR_A_PCH_D;
    PORTB = (PORTB & ~(MOTOR_B_PCH_B)) | MOTOR_B_NCH_B;
  }
}

void set_motor(void *v)
{
  switch ((int)v) {
  default:
    v = 0;
  case 0:
    set_led_state(true, false, 0);
    set_motor_power(0);
    break;
  case 1:
    set_led_state(false, true, 0);
    set_motor_power(192);
    break;
  case 2:
    set_led_state(false, true, 200);
    set_motor_power(-255);
    break;
  case 3:
    set_led_state(false, true, 0);
    set_motor_power(255);
    break;
  }
  after(3000, &set_motor, (void *)((int)v + 1));
}



void on_twi_data(unsigned char size, void const *ptr)
{
}

void setup()
{
  /* status lights are outputs */
  setup_leds();
  setup_motors();
  twi_set_callback(on_twi_data);
  //  kick off the chain of tasks
  set_motor(0);
}




int main(void) {
  setup_boot_code();

  setup();

  while (true) {
    schedule();
  }
  return 0;
}

