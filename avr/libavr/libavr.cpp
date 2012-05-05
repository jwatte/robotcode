#include "libavr.h"
#include "pins_avr.h"

/* global volatile int to avoid optimization of spinloop */
int volatile globalctr;

void fatal_no_blink(bool) {}
void (*g_fatal_blink)(bool) = &fatal_no_blink;

void fatal_set_blink(void (*blink)(bool))
{
  if (blink == NULL) {
    blink = &fatal_no_blink;
  }
  g_fatal_blink = blink;
}

/* tell the world about an emergency by flashing indicators */
void fatal(int err)
{
  disable_interrupts();
  //  remember what the last fatal code was
  if (eeprom_read_byte((uint8_t const *)EE_FATALCODE) != err) {
    eeprom_write_byte((uint8_t *)EE_FATALCODE, (unsigned char)err);
  }
  while (true)
  {
    if (err >= FATAL_TWI_ERROR_BASE) {
      /* just blink madly! */
      (*g_fatal_blink)(true);
      for (int volatile i = 0; i < 100; ++i) {
        for (globalctr = 0; globalctr < 300; ++globalctr) {
        }
      }
      (*g_fatal_blink)(false);
      for (int volatile i = 0; i < 100; ++i) {
        for (globalctr = 0; globalctr < 300; ++globalctr) {
        }
      }
    }
    else {
      for (int k = 0; k < err; ++k) {
        (*g_fatal_blink)(true);
        for (int volatile i = 0; i < 100; ++i) {
          for (globalctr = 0; globalctr < 500; ++globalctr) {
          }
        }
        (*g_fatal_blink)(false);
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

unsigned short g_lastTimer1Value;
unsigned short g_lastMillisecondValue;
unsigned short g_lastPhaseValue;

//  The timer runs at 65.536 milliseconds per lap,
//  but we call it 64 milliseconds even. There are 
//  1024 microseconds per millisecond... right? :-)
unsigned short read_timer1_inner()
{
  unsigned short timer1Value = TCNT1;
  g_lastPhaseValue += timer1Value - g_lastTimer1Value;
  g_lastTimer1Value = timer1Value;
  g_lastMillisecondValue += g_lastPhaseValue >> 10;
  g_lastPhaseValue &= 0x3ff;
  return timer1Value;
}

ISR(TIMER1_OVF_vect)
{
  //  this will magically update the timer
  read_timer1_inner();
}

ISR(TIMER1_COMPA_vect)
{
  read_timer1_inner();
}

unsigned short uread_timer()
{
  IntDisable idi;
  return read_timer1_inner();
}

unsigned short read_timer()
{
  IntDisable idi;
  read_timer1_inner();
  return g_lastMillisecondValue;
}

void setup_timers()
{
  power_timer1_enable();
  OCR1AH = 128;   //  interrupt half-way to keep counting
  OCR1AL = 0;
  ICR1H = 255;   //  run to "64 ms" for base 1024
  ICR1L = 255;
  TCCR1A = 0;
  //  timer 1 at 1 MHz
  TCCR1B = 2;
  TIMSK1 = (1 << OCIE1A) | (1 << TOIE0);
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

void delay(unsigned short ms)
{
  while (ms > 0) {
    udelay(999);
    --ms;
  }
}

/* task API */

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
  IntDisable idi;
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
    uint8_t idi = disable_interrupts();
    if ((short)(now - g_afters[ch].at_time) >= 0)
    {
      void (*func)(void*) = g_afters[ch].func;
      void *data = g_afters[ch].data;
      g_afters[ch].func = 0;
      g_afters[ch].data = 0;
      restore_interrupts(idi);
      (*func)(data);
      wdt_reset();
      unsigned short then = read_timer();
      if (then - now > MAX_TASK_TIME)
      {
        eeprom_write_word((word *)EE_TOO_LONG_TASK_PTR, (word)func);
        fatal(FATAL_TASK_TOOK_TOO_LONG);
      }
      now = then;
    }
    else {
      restore_interrupts(idi);
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
  unsigned short nboot = eeprom_read_word((uint16_t const *)EE_NUM_BOOTS);
  eeprom_write_word((uint16_t *)EE_NUM_BOOTS, ++nboot);
  wdt_reset();
}


/* TWI interface */

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
  //power_twi_enable();
  //TWAR = TWI_ADDRESS;
  //TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
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
  //  force interrupts on
  restore_interrupts(0x80);
}



int main(void) {
  setup_boot_code();
  setup();

  wdt_reset();
  wdt_enable(WDTO_8S);

  while (true) {
    schedule();
  }
  return 0;
}

