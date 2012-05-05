#if !defined(libavr_h)
#define libavr_h

#define F_CPU 8000000

#include <inttypes.h>
#include <string.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/wdt.h>
#include <avr/eeprom.h>
#include <util/atomic.h>
#include <util/twi.h>

#define EE_FATALCODE 0
#define EE_RESETSOURCE 1
#define EE_PREV_RESETSOURCE 2
#define EE_NUM_BOOTS 4
#define EE_TOO_LONG_TASK_PTR 6
#define EE_TOO_LONG_TASK_PRE 8
#define EE_TOO_LONG_TASK_POST 10

/* user-defined tuning constants */
#define EE_TUNING 32

//  how many milliseconds are allowed per task?
//  Note: the LCD is super slow!
#define MAX_TASK_TIME 2500

#define TWI_ADDRESS 0x01
#define TWI_MAX_SIZE 0x10

typedef uint8_t byte;
typedef uint8_t volatile regtype;
typedef uint16_t word;
typedef bool boolean;

/* fatal API */

enum
{
  FATAL_OUT_OF_AFTERS = 1,
  FATAL_BAD_DELAY_TIME = 2,
  FATAL_TASK_TOOK_TOO_LONG = 3,
  FATAL_TOO_LONG_UDELAY = 4,
  FATAL_BAD_PIN_ARG = 5,
  FATAL_BAD_PARAM = 6,
  FATAL_TWI_NO_INFO = 7,
  FATAL_TWI_SEND_TOO_BIG = 8,
  FATAL_TWI_ERROR_BASE = 0x60
};

/* calling fatal stops the program cold, recording the reason code in eeprom  */
void fatal(int err) __attribute__((noreturn));
void fatal_set_blink(void (*func)(bool on));

/* timer API */

void setup_timers();
unsigned short uread_timer();
unsigned short read_timer();
void udelay(unsigned short us);
void delay(unsigned short ms);


/* task API */

void at(unsigned short time, void (*func)(void *data), void *data);
void after(unsigned short delay, void (*func)(void *data), void *data);

extern int volatile globalctr;

/* TWI interface */

void twi_set_callback(void (*func)(unsigned char, void const *));
unsigned char twi_poll();
void *twi_getbuf();
bool twi_ready_to_send();
void twi_send(unsigned char bytes, void const *data);


/* your replacement for "main" */
void setup();

#endif  //  libavr_h

