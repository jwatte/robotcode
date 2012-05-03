
#define F_CPU 16000000UL

#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <avr/power.h>
#include <avr/wdt.h>

#define disable_interrupts() cli()
#define enable_interrupts() sei()

/* fatal API */

enum
{
  FATAL_OUT_OF_AFTERS = 1,
  FATAL_BAD_DELAY_TIME = 2,
  FATAL_TASK_TOOK_TOO_LONG = 3
};

void fatal(int err)
{
  disable_interrupts();
  while (true)
  {
    // do nothing, wait for watchdog
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
  if (g_timer_8us >= 124) {
    g_timer_8us -= 124;
    g_timer_1ms += 2;
  }
  else {
    g_timer_1ms += 1;
    g_timer_8us += 3;
  }
}

void setup_timers()
{
  //  timer 0 at 1000 Hz
  power_timer0_enable();
  TCCR0A = 0;
  TCCR0B = 3; //  system clock divided by 64 -- 250 kHz counter, so about 1 kHz clock
  TIMSK0 = 1; //  interrupt on overflow
}

short read_timer()
{
  disable_interrupts();
  unsigned short l = g_timer_1ms;
  unsigned short sh = TCNT0 * 4 + g_timer_8us * 8;
  enable_interrupts();
  if (sh >= 1000) {
    l += 1;
  }
  return l;
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
  MCUSR = 0;
  //  disable watchdog
  wdt_disable();
  //  TODO: enable watchdog at 2 second timeout
}



/* serial API */

#define BAUD_RATE 57600
#define BAUD_SCALE ((F_CPU / (BAUD_RATE * 16UL)) - 1)

char g_serbuf_in[31];
unsigned char g_serbuf_in_end;
char g_serbuf_out[31];
unsigned char g_serbuf_out_beg;
unsigned char g_serbuf_out_end;

void setup_serialport()
{
/*
  UCSRB |= ((1 << RXEN) | (1 << TXEN)); //  turn on
  UCSRC |= ((1 << URSEL) | (1 << UCSZ0) | (1 << UCSZ1)); // 8N1
  UBRRH = (BAUD_SCALE >> 8);
  UBRRL = (BAUD_SCALE & 0xff);
*/
}

ISR(USART_RX_vect)
{
}



/* boot stuff */

void setup_boot_code() {
  disable_interrupts();
  setup_watchdog();
  setup_timers();
  setup_serialport();
  enable_interrupts();
}




void set_led(void *arg)
{
  if (arg) {
    PORTB |= (1<<PB5); /* LED off */
  }
  else {
    PORTB &= ~(1<<PB5); /* LED off */
  }
  after(1000, set_led, arg ? 0 : (void *)1);
}

void setup()
{
  DDRB |= (1<<PB5); /* set PB5 to output */
  //  kick off the chain of tasks
  set_led(0);
}




int main(void) {
  setup_boot_code();

  setup();

  while (true) {
    schedule();
  }
  return 0;
}

